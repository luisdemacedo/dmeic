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
  answerType = _UNKNOWN_;
  buildSolversMO();
#pragma omp parallel for
  for (int i = 0; i < workers.size(); i++)
    workers[i].solver->setConfBudget(conflict_limit);

  YPoint yp{};
  for (int i = 0; i < getFormula()->nObjFunctions(); i++)
    yp.push_back(getFormula()->getUB(i) - getFormula()->getLB(i));

  pareto::max = yp;
  pareto::min = YPoint(getFormula()->nObjFunctions());
  pareto::hv_total = pareto::hv_shift(pareto::min, pareto::max);

  // encode obj functions
  bool resform[workers.size()];
#pragma omp parallel for
  for (size_t wid = 0; wid < workers.size(); wid++)
    resform[wid] = updateMOFormulationIfSAT(wid);

  if (std::all_of(resform, resform + workers.size(),
                  [](bool v) { return v; })) {
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
  waitingList->insert(yp);

  std::atomic<int> active{0};

#pragma omp parallel num_threads(workers.size())
  {
    const size_t wid = omp_get_thread_num();
    while (true) {
      std::optional<YPoint> yp;
      bool finished = false;

#pragma omp critical(work_state)
      {
        yp = waitingList->try_pop();

        if (!yp) {
          if (active.load() == 0)
            finished = true;
        }
      }

      if (finished) {
        break;
      }
      exploreFencedRegion(wid, *yp);
    }
  }
}

void ParUnsatSatMO::exploreFencedRegion(size_t wid, const YPoint &yp) {
  Worker &w = workers[wid];
  w.assumptions.clear();
  YPoint ul = yp;
  double runtime = cpuTime();

  std::ostringstream oss;
  oss << ul;
  std::osyncstream(std::cout)
      << getSolverId() << "c new harvest. upperLimit: " << oss.str() << "\n";

  assumeDominatingRegion(wid, ul);

  while (solve(wid) == l_True) {
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
    }
  }

  std::vector<YPoint> newULs = generateExpansionPoints(wid, ul);
  shareClauses(wid);
  shareSolutions(wid, true);
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
                                                           const YPoint &yp) {}
// bool ParUnsatSatMO::extendUL(uint64_t *upperObjv, uint64_t *upperObjix) {
//   bool extend = false;
//   vec<Lit> conflict;
//   Lit lit;
//   int iObj;
//   solver->conflict.copyTo(conflict);
//   while (conflict.size() > 0) {
//     lit = conflict.last();
//     conflict.pop();
//     iObj = getIObjFromLit(lit);
//     if (iObj > -1) {
//       if (upperObjix[iObj] < (*objRootLits[iObj]).size() - 1) {
//         extend = true;
//         upperObjix[iObj]++;
//         upperObjv[iObj] = (*objRootLits[iObj])[upperObjix[iObj]].first;
//       }
//     }
//   }
//
//   return extend;
// }
// bool ParUnsatSatMO::extendUL(YPoint &yp) {
//   int nObj = yp.size();
//   uint64_t upperObjv[nObj];
//   uint64_t upperObjix[nObj];
//
//   for (uint64_t i = 0; i < yp.size(); i++) {
//     upperObjv[i] = yp[i] + 1;
//   }
//   evalToIndex(upperObjv, upperObjix);
//   bool res = extendUL(upperObjv, upperObjix);
//
//   for (uint64_t i = 0; i < yp.size(); i++) {
//     yp[i] = upperObjv[i] - 1;
//   }
//   return res;
// }
