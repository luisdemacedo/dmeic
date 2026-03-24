
// #define PARTIAL
#ifndef PARTIAL

#include "Alg_PMinimalMO.h"
#include <algorithm> // std::max

using namespace openwbo;
// using namespace NSPACE;
using NSPACE::toLit;

void PMinimalMO::search_MO() {
  // Init Structures
  init();

  //     printf("\t\tc strategic_search\n");
  //     printf("c eps: %f\n", epsilon);

  // Build solver
  double epsthreshold = 1 + 1e-4;

  solver = buildSolverMO();

  bool resform, terminate = false;
  nbMCS = 0;

  answerType = _UNKNOWN_;

  bool permanentBlock = false;

  while (!terminate) {
    // encode obj functions

    resform = updateMOFormulationIfSAT();

    if (resform) {

      printf("c search\n");
      searchPMinimalMO();
      if (_stopSearch->load(std::memory_order_acquire)) {
        answerType = _INTERRUPTED_;
        printf(
            "After checking stopSearch (%p), another thread requested to stop "
            "the search. Thread %d stopping now...\n",
            _stopSearch, omp_get_thread_num());
        return;
      }

    } else {
      printf("c No more solutions!\n");
    }
    printf("c Done searching\n");
    printf("c epsilon: %f\n", epsilon);
    printf("c reductionFactor: %f\n", redFactor);
    if ((permanentBlock && !resform) || epsilon <= 1 || redFactor < 0) {
      terminate = true;
      printf("c time to terminate\n");
    } else {
      if (epsilon <= epsthreshold)
        epsilon = 1;
      else
        epsilon = 1 + (epsilon - 1) / redFactor;
      printf("c REENCODE epsilon = %f\n", epsilon);
    }
  }

  if (nondom.size() > 0) {

    if (epsilon <= 1) {
      printf("c LBset = PF\n");
      clearLowerBoundSet();
      for (size_t i = 0; i < nondom.size(); i++)
        updateLowerBoundSet(nondom[i], false);
    }
    answerType = _OPTIMUM_;
  } else {
    if (nreencodes == 1)
      clearLowerBoundSet();
  }

  printf("I am thread %d setting pointer stopSearch (%p) to true\n",
         omp_get_thread_num(), _stopSearch);
  _stopSearch->store(true, std::memory_order_release);
  printAnswer(answerType);
}

bool PMinimalMO::searchPMinimalMO() {
  double runtime = cpuTime();
  int nObj = maxsat_formula->nObjFunctions();

  YPoint ul(nObj);

  assumptions.clear();

  if (_stopSearch->load(std::memory_order_acquire)) {
    printf("After checking stopSearch (%p), another thread requested to stop "
           "the search. Thread %d stopping now...\n",
           _stopSearch, omp_get_thread_num());
    return false;
  }

  for (auto sat = solve(); sat == l_True; sat = solve()) {
    if (_stopSearch->load(std::memory_order_acquire)) {
      printf("After checking stopSearch (%p), another thread requested to stop "
             "the search. Thread %d stopping now...\n",
             _stopSearch, omp_get_thread_num());
      return false;
    }

    for (; sat == l_True; sat = solve()) {
      if (_stopSearch->load(std::memory_order_acquire)) {
        printf(
            "After checking stopSearch (%p), another thread requested to stop "
            "the search. Thread %d stopping now...\n",
            _stopSearch, omp_get_thread_num());
        return false;
      }

      Model m = make_model(solver->model);
      solution().pushSafe(m);
      ul = solution().yPoint();
      blockDominatedRegion(ul);
      printf("c o ");
      std::cout << ul << std::endl;
      runtime = cpuTime();
      printf("c new feasible solution (time: %.3f)\n", runtime - initialTime);
      assumptions.clear();
      PBtoCNF::assumeDominatingRegion(ul);
    }
    assumptions.clear();
    runtime = cpuTime();
    printf("c new optimal solution (time: %.3f)\n", runtime - initialTime);
    blockDominatedRegion(ul);
  }

  if (solution().size() == 0) {
    answerType = _UNSATISFIABLE_;
    return false;
  } else {
    answerType = _OPTIMUM_;
  }
  return true;
}

#endif
#undef PARTIAL
