
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

      printf("%sc search\n", getSolverId().c_str());
      searchPMinimalMO();
      if (getStopSearchFlag()) {
        answerType = _INTERRUPTED_;
        DLOG(stdout,
             "%sstopSearch has been set to true, another thread requested to "
             "stop the search. Search stopped.\n",
             getSolverId().c_str());
        return;
      }

    } else {
      printf("%sc No more solutions!\n", getSolverId().c_str());
    }
    printf("%sc Done searching\n", getSolverId().c_str());
    printf("%sc epsilon: %f\n", getSolverId().c_str(), epsilon);
    printf("%sc reductionFactor: %f\n", getSolverId().c_str(), redFactor);
    if ((permanentBlock && !resform) || epsilon <= 1 || redFactor < 0) {
      terminate = true;
      printf("%sc time to terminate\n", getSolverId().c_str());
    } else {
      if (epsilon <= epsthreshold)
        epsilon = 1;
      else
        epsilon = 1 + (epsilon - 1) / redFactor;
      printf("%sc REENCODE epsilon = %f\n", getSolverId().c_str(), epsilon);
    }
  }

  if (nondom.size() > 0) {

    if (epsilon <= 1) {
      printf("%sc LBset = PF\n", getSolverId().c_str());
      clearLowerBoundSet();
      for (size_t i = 0; i < nondom.size(); i++)
        updateLowerBoundSet(nondom[i], false);
    }
    answerType = _OPTIMUM_;
  } else {
    if (nreencodes == 1)
      clearLowerBoundSet();
  }

  if (solution().size() == 0)
    answerType = _UNSATISFIABLE_;

  requestStopSearch();
  shareSolutions(getShareSolutions());
  if (!isInsidePortfolio() || !getShareSolutions()) {
    printAnswer(answerType);
    // exit(answerType);
  }
}

bool PMinimalMO::searchPMinimalMO() {
  double runtime = cpuTime();
  int nObj = maxsat_formula->nObjFunctions();

  YPoint ul(nObj);

  assumptions.clear();

  if (getStopSearchFlag()) {
    DLOG(stdout,
         "%sstopSearch has been set to true, another thread requested to "
         "stop the search. Stopping search now...\n",
         getSolverId().c_str());
    return false;
  }

  shareClauses();
  shareSolutions(getShareSolutions());
  auto sat = solve();
  while (sat == l_Undef) {
    printf("%sc budget exhausted. Retrying...\n", getSolverId().c_str());

    if (getStopSearchFlag()) {
      DLOG(stdout,
           "%sstopSearch has been set to true, another thread requested to "
           "stop the search. Stopping search now...\n",
           getSolverId().c_str());
      return false;
    }
    shareClauses();
    shareSolutions(getShareSolutions());
    sat = solve();
  }
  for (; sat == l_True;) {
    if (getStopSearchFlag()) {
      DLOG(stdout,
           "%sstopSearch has been set to true, another thread requested to "
           "stop the search. Stopping search now...\n",
           getSolverId().c_str());
      return false;
    }

    for (; sat == l_True;) {
      if (getStopSearchFlag()) {
        DLOG(stdout,
             "%sstopSearch has been set to true, another thread requested to "
             "stop the search. Stopping search now...\n",
             getSolverId().c_str());
        return false;
      }

      Model m = make_model(solver->model);
      solution().pushSafe(m);
      ul = solution().yPoint();
      blockDominatedRegion(ul);
      std::ostringstream oss;
      oss << ul;
      std::osyncstream(std::cout)
          << getSolverId() << "c o " << oss.str() << "\n";
      // printf("%sc o ", getSolverId().c_str());  TODO: check this
      // std::cout << ul << std::endl;
      runtime = cpuTime();
      printf("%sc new feasible solution (time: %.3f)\n", getSolverId().c_str(),
             runtime - initialTime);
      assumptions.clear();
      PBtoCNF::assumeDominatingRegion(ul);

      shareClauses();
      shareSolutions(getShareSolutions());
      sat = solve();
      while (sat == l_Undef) {
        printf("%sc budget exhausted. Retrying...\n", getSolverId().c_str());
        if (getStopSearchFlag()) {
          DLOG(stdout,
               "%sstopSearch has been set to true, another thread requested "
               "stop the search. Stopping search now...\n",
               getSolverId().c_str());
          return false;
        }
        shareClauses();
        shareSolutions(getShareSolutions());
        sat = solve();
      }
    }
    assumptions.clear();
    runtime = cpuTime();
    printf("%sc new optimal solution (time: %.3f)\n", getSolverId().c_str(),
           runtime - initialTime);
    blockDominatedRegion(ul);
    shareClauses();
    shareSolutions(getShareSolutions());
    sat = solve();
    while (sat == l_Undef) {
      printf("%sc budget exhausted. Retrying...\n", getSolverId().c_str());
      if (getStopSearchFlag()) {
        DLOG(stdout,
             "%sstopSearch has been set to true, another thread requested to "
             "stop the search. Stopping search now...\n",
             getSolverId().c_str());
        return false;
      }
      shareClauses();
      shareSolutions(getShareSolutions());
      sat = solve();
    }
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
