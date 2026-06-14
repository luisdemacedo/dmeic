#include "Alg_ParPMinimalMO.h"
#include <algorithm> // std::max

using namespace openwbo;
// using namespace NSPACE;
using NSPACE::toLit;

void ParPMinimalMO::search_MO() {
  // Init Structures
  init();
  initWorkers(omp_get_max_threads());

  //     printf("\t\tc strategic_search\n");
  //     printf("c eps: %f\n", epsilon);

  // Build solver
  double epsthreshold = 1 + 1e-4;

  buildSolversMO(); // TODO: maybe make this a parameter

  bool resform[workers.size()];
  bool terminate = false;
  nbMCS = 0;

  answerType = _UNKNOWN_;

  bool permanentBlock = false;

  while (!terminate) {
    // encode obj functions

#pragma omp parallel num_threads(workers.size())
    {
      if ((size_t)omp_get_num_threads() != workers.size()) {
        fprintf(stderr, "c ERROR: got %d threads, expected %zu\n",
                omp_get_num_threads(), workers.size());
        exit(_ERROR_);
      }
      resform[omp_get_thread_num()] =
          updateMOFormulationIfSAT(omp_get_thread_num());
    }

    if (std::all_of(resform, resform + workers.size(), std::identity{})) {
      printf("c search\n");
#pragma omp parallel num_threads(workers.size())
      searchParPMinimalMO();
    } else {
      printf("c No more solutions!\n");
    }
    printf("c Done searching\n");
    printf("c epsilon: %f\n", epsilon);
    printf("c reductionFactor: %f\n", redFactor);
    if ((permanentBlock &&
         std::none_of(resform, resform + workers.size(), std::identity{})) ||
        epsilon <= 1 || redFactor < 0) {
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
    int nreencodes = 0;
    for (size_t i = 0; i < workers.size(); i++)
      nreencodes += workers[i].nreencodes;
    if (nreencodes == 1)
      clearLowerBoundSet();
  }

  printAnswer(answerType);
}

bool ParPMinimalMO::searchParPMinimalMO() {
  size_t wid = omp_get_thread_num();
  auto &w = workers[wid];
  double runtime = cpuTime();
  int nObj = maxsat_formula->nObjFunctions();

  YPoint ul(nObj);
  std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<double> dist(0.0, 1.0);

  lbool sat;
  do { // Randomizing the starting point
    vec<Lit> core;
    for (size_t i = 0; i < w.solver->conflict.size(); i++)
      core.push(w.solver->conflict[i]);

    if (core.size())
      w.solver->addClause(core);

    w.assumptions.clear();
    for (size_t i = 0; i < nObj; i++) {
      for (size_t j = 0; j < getFormula()->getObjFunction(i)->_lits.size(); j++)
        if (dist(rng) < 0.1)
          w.assumptions.push(getFormula()->getObjFunction(i)->_lits[j]);
    }
  } while ((sat = solve(wid)) != l_True &&
           (sat == l_Undef || w.solver->conflict.size() > 0));

  for (; sat == l_True;) {
    for (; sat == l_True;) {
      Model m = make_model(w.solver->model);
      YPoint yp = evalModel(m);
      w.solutions.push(m);
      ul = yp;
      shareClauses(wid);
      shareSolutions(wid, true);
      blockDominatedRegion(wid, ul);
      std::ostringstream oss;
      oss << ul;
      std::osyncstream(std::cout)
          << getSolverId() << "c o " << oss.str() << "\n";
      runtime = cpuTime();
      printf("%sc new feasible solution (time: %.3f)\n", getSolverId().c_str(),
             runtime - initialTime);
      w.assumptions.clear();
      assumeDominatingRegion(wid, ul);

      shareClauses(wid);
      shareSolutions(wid, true);
      sat = solve(wid);
      while (sat == l_Undef) {
        shareClauses(wid);
        shareSolutions(wid, true);
        sat = solve(wid);
      }
    }
    w.assumptions.clear();
    runtime = cpuTime();
    printf("%sc new optimal solution (time: %.3f)\n", getSolverId().c_str(),
           runtime - initialTime);
    blockDominatedRegion(wid, ul);
    shareClauses(wid);
    shareSolutions(wid, true);
    sat = solve(wid);
    while (sat == l_Undef) {
      shareClauses(wid);
      shareSolutions(wid, true);
      sat = solve(wid);
    }
  }

  if (solution().size() == 0 && sharedSolutions->empty()) {
    answerType = _UNSATISFIABLE_;
    return false;
  } else {
    answerType = _OPTIMUM_;
  }
  return true;
}
