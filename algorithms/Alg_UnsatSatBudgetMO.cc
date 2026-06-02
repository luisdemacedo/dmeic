#include "Alg_UnsatSatBudgetMO.h"
#include "../Pareto.h"
#include "core/SolverTypes.h"
#include <cstdint>
#include <iostream>
using namespace openwbo;
using namespace std;

vector<MyPartition> UnsatSatBudgetMO::generate() {
  int nObj = getFormula()->nObjFunctions();
  vector<MyPartition> partitions{};
  partitions.reserve(nObj);
  for (int i = 0; i < nObj; i++)
    partitions.emplace_back(getFormula()->getObjFunction(i), 30);
  return partitions;
}

MyPartition UnsatSatBudgetMO::mix(vector<MyPartition> partitions) {
  MyPartition mix{};
  random_device rd{};
  mt19937 rng{rd()};
  uniform_int_distribution<int> uni(0, partitions.size() - 1);
  int rnd{};
  while (partitions.size()) {
    uniform_int_distribution<int> uni(0, partitions.size() - 1);
    rnd = uni(rng);
    mix.push(partitions[rnd].pop());

    if (partitions[rnd].size() == 0) {
      partitions[rnd] = partitions[partitions.size() - 1];
      partitions.pop_back();
    }
  }
  return mix;
}

bool UnsatSatBudgetMO::buildWorkFormula() {
  // Init Structures
  bool ret = updateMOFormulationIfSAT();
  return ret;
}

void UnsatSatBudgetMO::search_MO() {
  // Build solver
  build();
  // encode obj functions
  bool resform = buildWorkFormula();

  if (resform) {
    printf("%sc search\n", getSolverId().c_str());
    searchUnsatSatMO();
  } else {
    printf("%sc No more solutions!\n", getSolverId().c_str());
  }
  if (getStopSearchFlag()) {
    answerType = _INTERRUPTED_;
    printf("%sstopSearch has been set to true, another thread requested to "
           "stop the search. Search stopped.\n",
           getSolverId().c_str());
    return;
  }

  printf("%sc Done searching\n", getSolverId().c_str());
  PBtoCNF::consolidateSolution();
  if (solution().size() > 0) {
    answerType = _OPTIMUM_;
  } else {
    answerType = _UNSATISFIABLE_;
  }

  requestStopSearch();
  shareSolutions(getShareSolutions());
  if (!isInsidePortfolio() || !getShareSolutions()) {
    printAnswer(answerType);
    // exit(answerType);
  }
}

bool UnsatSatBudgetMO::rootedSearch(const YPoint &yp) {
  double runtime = cpuTime();
  assumptions.clear();
  YPoint ul = yp;

newHarvest:
  std::ostringstream oss;
  oss << ul;
  std::osyncstream(std::cout)
      << getSolverId() << "c new harvest. upperLimit: " << oss.str() << "\n";
  assumptions.clear();
  // reinserts the MSU3 blocked vars

  for (const auto &el : blockedVars)
    assumptions.push(~el);
  assumeDominatingRegion(ul);

  if (getStopSearchFlag()) {
    printf("%sstopSearch has been set to true, another thread requested to "
           "stop the search. Stopping search now...\n",
           getSolverId().c_str());
    return false;
  }
  shareClauses();
  shareSolutions(getShareSolutions());
  while (solve() == l_True) {
    if (getStopSearchFlag()) {
      printf("%sstopSearch has been set to true, another thread requested to "
             "stop the search. Stopping search now...\n",
             getSolverId().c_str());
      return false;
    }
    Model m = make_model(solver->model);
    // Only block dominated region if m1 gets into the Solution
    if (solution().pushSafe(m)) {
      blockStep(solution().yPoint());
      std::ostringstream oss;
      oss << solution().yPoint();
      std::osyncstream(std::cout)
          << getSolverId() << "c o " << oss.str() << "\n";
      // printf("%sc o ", getSolverId().c_str());
      // std::cout << solution().yPoint() << std::endl;
      shareSolutions(getShareSolutions());
      runtime = cpuTime();
      printf("%sc new optimal solution (time: %.3f)\n", getSolverId().c_str(),
             runtime - initialTime);
    } else {
      auto yp = MOCO::evalModel(m);
      for (auto &el : solution()) {
        auto yp1 = el.second.first.yPoint();
        if (pareto::dominates(yp1, yp)) {
          blockStep(yp1);
          std::ostringstream oss;
          oss << yp1;
          std::osyncstream(std::cout)
              << getSolverId() << "c o " << oss.str() << "\n";
          // printf("%sc o ", getSolverId().c_str());
          // std::cout << yp1 << std::endl;
          printf("%sc old solution (time: %.3f)\n", getSolverId().c_str(),
                 runtime - initialTime);
        }
      }
    }
  }
  if (getStopSearchFlag()) {
    printf("%sstopSearch has been set to true, another thread requested to "
           "stop the search. Stopping search now...\n",
           getSolverId().c_str());
    return false;
  }
  if (extendUL(ul))
    goto newHarvest;
  return true;
}

bool UnsatSatBudgetMO::searchUnsatSatMO() {
  int nObj = getFormula()->nObjFunctions();
  YPoint ul(nObj);
  // for(int i = 0; i < nObj; i++){
  //   ul[i] = getTighterLB(i);
  // }

  auto dom = pareto::dominator(solution());
  if (pareto::dominates(ul, dom))
    ul = dom;
  rootedSearch(ul);
  if (getStopSearchFlag()) {
    printf("%sstopSearch has been set to true, another thread requested to "
           "stop the search. Stopping search now...\n",
           getSolverId().c_str());
    return false;
  }
  if (solution().size() == 0) {
    answerType = _UNSATISFIABLE_;
    return false;
  } else {
    answerType = _OPTIMUM_;
  }
  return true;
}

bool UnsatSatBudgetMO::extendUL(uint64_t *upperObjv, uint64_t *upperObjix) {
  bool extend = false;
  vec<Lit> conflict;
  Lit lit;
  int iObj;
  solver->conflict.copyTo(conflict);
  set<Lit> done;
  toggleConflictBudget(conflict_core);
  optimize_core_destructive(conflict, done);
  toggleConflictBudget();
  while (conflict.size() > 0) {
    lit = conflict.last();
    conflict.pop();
    iObj = getIObjFromLit(lit);
    if (iObj > -1) {
      if (upperObjix[iObj] < (*objRootLits[iObj]).size() - 1) {
        extend = true;
        upperObjix[iObj]++;
        upperObjv[iObj] = (*objRootLits[iObj])[upperObjix[iObj]].first;
      }
    }
  }

  return extend;
}
bool UnsatSatBudgetMO::extendUL(YPoint &yp) {
  int nObj = yp.size();
  uint64_t upperObjv[nObj];
  uint64_t upperObjix[nObj];

  for (uint64_t i = 0; i < yp.size(); i++) {
    upperObjv[i] = yp[i] + 1;
  }
  evalToIndex(upperObjv, upperObjix);
  bool res = extendUL(upperObjv, upperObjix);

  for (uint64_t i = 0; i < yp.size(); i++) {
    yp[i] = upperObjv[i] - 1;
  }
  return res;
}

bool UnsatSatBudgetMO::optimize_core_destructive(vec<Lit> &conflict,
                                                 set<Lit> &done) {
  lbool sat{};
  for (int i = 0, n = conflict.size(); i < n; i++) {
    assumptions.clear();
    if (done.count(conflict[i]))
      continue;
    auto x = conflict[i];
    for (int j = 0; j < n; j++)
      if (j != i)
        assumptions.push(~conflict[j]);

    if (getStopSearchFlag()) {
      printf("%sstopSearch has been set to true, another thread requested to "
             "stop the search. Stopping search now...\n",
             getSolverId().c_str());
      return false;
    }

    shareClauses();
    shareSolutions(getShareSolutions());
    if ((sat = solve()) == l_True) {
      if (getStopSearchFlag()) {
        printf("%sstopSearch has been set to true, another thread requested to "
               "stop the search. Stopping search now...\n",
               getSolverId().c_str());
        return false;
      }

      done.insert(x);
      Model m = make_model(solver->model);
      // Only block dominated region if m1 gets into the Solution
      if (solution().pushSafe(m)) {
        // block region dominated by last point
        auto sol = solution().oneSolution();
        auto yp = sol.yPoint();
        // create slide variable for newly found solution
        std::ostringstream oss;
        oss << sol;
        std::osyncstream(std::cout)
            << getSolverId() << "c o " << oss.str() << "\n";
        // printf("%sc o ", getSolverId().c_str());
        // std::cout << sol << std::endl;
        printf("%sc new satisfiable solution\n", getSolverId().c_str());
        if (_block)
          blockStep(yp);
        else
          printf("%sc solution left unblocked\n", getSolverId().c_str());
      } else {
        auto yp = MOCO::evalModel(m);
        std::ostringstream oss;
        oss << yp;

        std::osyncstream(std::cout)
            << getSolverId() << "c o " << oss.str() << "\n";
        // printf("%sc o ", getSolverId().c_str());
        // std::cout << yp << std::endl;
        printf("%sc old solution\n", getSolverId().c_str());
      }
    } else if (sat == l_Undef) {
      cout << getSolverId() << "c budget exhausted during core optimization"
           << endl;
      answerType = _BUDGET_;
      return false;
    } else {
      solver->conflict.copyTo(conflict);
      return optimize_core_destructive(conflict, done);
    }
  }
  return true;
}
