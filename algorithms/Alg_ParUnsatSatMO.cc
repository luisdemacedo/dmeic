#include "Alg_ParUnsatSatMO.h"
#include "../Pareto.h"
#include "core/SolverTypes.h"
#include <algorithm> // std::max
#include <cstdint>
#include <iostream>
#include <memory>
using namespace openwbo;

// vector<MyPartition> ParUnsatSatMO::generate() {
// }

// MyPartition ParUnsatSatMO::mix(vector<MyPartition> partitions) {
// }

void ParUnsatSatMO::search_MO() {
  // Build solver
  init();
  initWorkers();
  answerType = _UNKNOWN_;
  buildSolversMO();

  YPoint yp{};
  for (int i = 0; i < getFormula()->nObjFunctions(); i++)
    yp.push_back(getFormula()->getUB(i) - getFormula()->getLB(i));

  pareto::max = yp;
  pareto::min = YPoint(getFormula()->nObjFunctions());
  pareto::hv_total = pareto::hv_shift(pareto::min, pareto::max);

  // encode obj functions
  bool resform[workers.size()];
#pragma omp parallel num_threads(workers.size())
  {
    assert(static_cast<std::size_t>(omp_get_num_threads()) == workers.size());
    size_t wid = omp_get_thread_num();
    resform[wid] = updateMOFormulationIfSAT(wid);

    if (resform[wid]) {
      consolidateSolution(wid);
      blockDominatedRegion(wid, workers[wid].first.yPoint());
    }

#pragma omp barrier
    if (resform[wid]) {
      shareClauses(wid);
      shareSolutions(wid, true);
    }
  }

  bool feasible = resform[0];

  assert(std::all_of(resform, resform + workers.size(),
                     [feasible](bool v) { return v == feasible; }));

  if (feasible) {
    printf("c search\n");
    searchUnsatSatMO();
  } else {
    printf("c No more solutions!\n");
  }
  printf("c Done searching\n");
  // PBtoCNF::consolidateSolution();
  if (!sharedSolutions->empty()) {
    answerType = _OPTIMUM_;
  } else {
    answerType = _UNSATISFIABLE_;
  }

  printAnswer(answerType);
}

bool ParUnsatSatMO::rootedSearch(const YPoint &yp) {
  double runtime = cpuTime();
  lbool sat{};
  waiting_list->insert(yp);

  std::atomic<int> active{0};

#pragma omp parallel num_threads(workers.size())
  {
    const size_t wid = omp_get_thread_num();
    while (true) {
      std::optional<YPoint> yp;
      bool finished = false;

#pragma omp critical(work_state)
      {
        yp = waiting_list->try_pop();

        if (yp)
          active.fetch_add(1);
        else if (active.load() == 0)
          finished = true;
      }

      if (finished)
        break;

      if (!yp)
        continue;

      exploreFencedRegion(wid, *yp);
      active.fetch_sub(1);
    }
  }
  return true;
}

void ParUnsatSatMO::exploreFencedRegion(size_t wid, const YPoint &yp) {
  Worker &w = workers[wid];
  w.assumptions.clear();
  YPoint ul = yp;
  double runtime = cpuTime();
  lbool sat{};

  std::ostringstream oss;
  oss << ul;
  std::osyncstream(std::cout)
      << getSolverId() << "c new harvest. upperLimit: " << oss.str() << "\n";

  assumeDominatingRegion(wid, ul);

  while ((sat = solve(wid)) == l_Undef) {
    printf("%sc budget exhausted. Retrying...\n", getSolverId().c_str());
    shareClauses(wid);
    shareSolutions(wid, true);
  }
  while (sat == l_True) {
    Model m = make_model(w.solver->model);
    // Only block dominated region if m1 gets into the Solution
    if (w.solutions.pushSafe(m)) {
      // if (timestats[_time1stSol_] < 0) { // Uninitialized, first solution
      // found
      //   runtime = cpuTime();
      //   timestats[_time1stSol_] = runtime - initialTime;
      //   runstats[_nsatcalls1stSol_] = nbSatisfiable;
      // }
      YPoint newYPoint = evalModel(m);
      blockStep(wid, newYPoint);
      std::ostringstream oss;
      oss << newYPoint;
      std::osyncstream(std::cout)
          << getSolverId() << "c o " << oss.str() << "\n";
      runtime = cpuTime();
      printf("%sc new optimal solution (time: %.3f)\n", getSolverId().c_str(),
             runtime - initialTime);
      shareClauses(wid);
      shareSolutions(wid, true);
    }
    while ((sat = solve(wid)) == l_Undef) {
      printf("%sc budget exhausted. Retrying...\n", getSolverId().c_str());
      shareClauses(wid);
      shareSolutions(wid, true);
    }
  }

  shareClauses(wid);
  shareSolutions(wid, true);

  std::vector<YPoint> newULs = generateExpansionPoints(wid, ul);
#pragma omp critical(work_state)
  {
    for (const auto &newUL : newULs)
      waiting_list->insert(newUL);
  }
}

bool ParUnsatSatMO::searchUnsatSatMO() {
  int nObj = getFormula()->nObjFunctions();
  YPoint ul(nObj);

  auto dom = pareto::dominator(solution());
  if (pareto::dominates(ul, dom))
    ul = dom;
  rootedSearch(ul);
  if (sharedSolutions->empty()) {
    answerType = _UNSATISFIABLE_;
    return false;
  } else {
    answerType = _OPTIMUM_;
  }
  return true;
}

std::vector<YPoint> ParUnsatSatMO::generateExpansionPoints(size_t wid,
                                                           const YPoint yp) {
  int nObj = yp.size();
  uint64_t upperObjv[nObj];
  uint64_t upperObjix[nObj];

  for (uint64_t i = 0; i < yp.size(); i++)
    upperObjv[i] = yp[i] + 1;

  evalToIndex(wid, upperObjv, upperObjix);
  bool res = extendUL(wid, upperObjv, upperObjix);
  std::vector<YPoint> newULs = {};
  if (res) {
    YPoint newUL = yp;
    for (uint64_t i = 0; i < yp.size(); i++)
      upperObjv[i]--;
    for (size_t i = 0; i < nObj; i++) {
      const uint64_t max = getFormula()->getUB(i) - getFormula()->getLB(i);
      if (upperObjv[i] == yp[i])
        continue;
      newUL[i] = std::min(newUL[i] + stride * (upperObjv[i] - newUL[i]), max);
      newULs.push_back(newUL);
      newUL[i] = yp[i];
    }
  }
  return newULs;
}
bool ParUnsatSatMO::extendUL(size_t wid, uint64_t *upperObjv,
                             uint64_t *upperObjix) {
  Worker &w = workers[wid];
  bool extend = false;
  vec<Lit> conflict;
  Lit lit;
  int iObj;
  w.solver->conflict.copyTo(conflict);
  while (conflict.size() > 0) {
    lit = conflict.last();
    conflict.pop();
    iObj = getIObjFromLit(wid, lit);
    if (iObj > -1) {
      if (upperObjix[iObj] < (*w.objRootLits[iObj]).size() - 1) {
        extend = true;
        upperObjix[iObj]++;
        upperObjv[iObj] = (*w.objRootLits[iObj])[upperObjix[iObj]].first;
      }
    }
  }

  return extend;
}
