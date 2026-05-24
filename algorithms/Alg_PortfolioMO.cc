#include "Alg_PortfolioMO.h"

namespace openwbo {

StatusCode PortfolioMO::search() {
#pragma omp parallel for
  for (size_t idx = 0; idx < _portfolio.size(); idx++) {
    _portfolio[idx]->search();
  }

  for (size_t idx = 0; idx < _portfolio.size(); idx++) {
    DLOG(stderr, "Answer from solver %zu: %u\n", idx,
         _portfolio[idx]->answerType);
    if (_portfolio[idx]->answerType != _INTERRUPTED_) {
      this->answerType = _portfolio[idx]->answerType;
    }
  }

  DLOG(stderr, "Answer from portfolio: %u\n", this->answerType);

  printAnswer(this->answerType);
  return this->answerType;
  // portfolio result
}

void PortfolioMO::search_MO() {
  assert(false && "PortfolioMO::search_MO() should not be called");
}

void PortfolioMO::interruptSolver() {
  for (auto &solver : _portfolio)
    solver->interruptSolver();
  this->answerType = _INTERRUPTED_;
  printAnswer(answerType);
}

void PortfolioMO::printAnswer(int type) {
  switch (answerType) {
  case _SATISFIABLE_:
    printf("s SATISFIABLE\n");
    printSolutions();
    printStats();
    break;

  case _OPTIMUM_:
    printf("s OPTIMUM FOUND\n");
    printSolutions();
    printStats();
    break;
  case _UNSATISFIABLE_:
    printf("s UNSATISFIABLE\n");
    break;
  case _UNKNOWN_:
    printf("s UNKNOWN\n");
    break;
  case _INTERRUPTED_:
    printf("s INTERRUPTED\n");
    printSolutions();
    printStats();
    break;
  case _MEMOUT_:
    printf("s MEMOUT\n");
    printSolutions();
    printStats();
    break;
  default:
    printf("c Error: Invalid answer type.\n");
    break;
  }
}

void PortfolioMO::printSolutions() { // TODO: Allow printing to a file

  FILE *f = stdout;

  auto sols = sharedSolutions->getSolutions();

  if (print_model && getShareSolutions()) {
    fprintf(f, "c -------- Portfolio models (%zu):\n", sols.size());
    for (auto &[t_id, sol] : sols) {
      Model m = sol.model();
      fprintf(f, "[s%zu] v", t_id);
      for (int i = 0; i < maxsat_formula->nVars(); i++) {
        indexMap::const_iterator it = maxsat_formula->getIndexToName().find(i);
        if (it != maxsat_formula->getIndexToName().end()) {
          fprintf(f, " ");
          if (m[i] == l_True)
            fprintf(f, " ");
          else
            fprintf(f, "-");
          fprintf(f, "%s", it->second.c_str());
        }
      }
      fprintf(f, "\n");
    }
    fprintf(f, "c --- End of models\n");
  }

  if (getShareSolutions()) {
    fprintf(f, "c Objective values:\n");
    fprintf(f, "c Portfolio objv (%zu):\n", sols.size());
    for (auto &[t_id, sol] : sols) {
      fprintf(f, "c [s%zu] pt", t_id);
      for (int i = 0; i < maxsat_formula->nObjFunctions(); i++)
        fprintf(f, " %zu", sol.yPoint()[i]);

      fprintf(f, "\n");
    }
    fprintf(f, "c End of objective values\n");
  }
}

void PortfolioMO::printStats() {
  double totalTime = cpuTime();

  FILE *f = stdout;
  // fprintf(f,
  //         "%-8s clrunstats %18s %12s %12s %12s %12s %12s %18s %12s %18s %18s
  //         "
  //         "%8s %8s "
  //         "%8s\n",
  //         "worker", "nsatcalls_1stSol", "nsatcalls", "ncalls", "n_eff_sols",
  //         "n_nondom", "n_prob_vars", "n_prob_clauses", "n_enc_vars(sum)",
  //         "n_enc_clauses(sum)", "n_enc_rootvars(sum)", "n_reencodes",
  //         "rapprox", "nobj");
  //
  // for (size_t idx = 0; idx < _portfolio.size(); idx++) {
  //   auto &stats = _portfolio[idx]->runstats;
  //   fprintf(
  //       f,
  //       "%-8s crunstats %12d %12d %12d %12d %12d %12d %18d %12d %18d %18d
  //       %18d "
  //       "%18.4f %10d\n",
  //       ("solver" + std::to_string(idx)).c_str(), stats[_nsatcalls1stSol_],
  //       stats[_nsatcalls_], stats[_ncalls_], stats[_neffsols_],
  //       stats[_nnondom_], stats[_nprobvars_], stats[_nprobclauses_],
  //       stats[_nencvars_], stats[_nencclauses_], stats[_nencrootvars_],
  //       stats[_nreencodes_], _portfolio[idx]->repsilon,
  //       getFormula()->nObjFunctions());
  // }
  // fprintf(f, "%-8s cltimestats %12s %12s\n", "worker", "time_1stSol",
  //         "totaltime");
  // for (size_t idx = 0; idx < _portfolio.size(); idx++) {
  //   auto &tstats = _portfolio[idx]->timestats;
  //   tstats[_totaltime_] = totalTime - _portfolio[idx]->initialTime;
  //   fprintf(f, "%-8s ctimestats %12.2f %12.2f\n",
  //           ("solver" + std::to_string(idx)).c_str(), tstats[_time1stSol_],
  //           tstats[_totaltime_]);
  // }
  //
  fprintf(f, "cclausesharingstats %20s %20s %20s %20s %20s %22s %24s %24s\n",
          "nsynccalls", "nnonempty_pushes", "nnonempty_grabs",
          "nclauses_pushed", "nclauses_grabbed", "nclauses_filtered_out",
          "time_spent_syncing (ms)", "time_waiting_lock (ms)");
  for (size_t idx = 0; idx < _portfolio.size(); idx++) {
    auto syncTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        sharedLearntClauses->getSyncTime(idx));
    auto lockWaitTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        sharedLearntClauses->getLockWaitTime(idx));

    fprintf(f,
            "%-20s %20zu %20zu %20zu %20zu %20zu %22zu %24lld "
            "%24lld\n",
            ("solver" + std::to_string(idx)).c_str(),
            sharedLearntClauses->getSyncs(idx),
            sharedLearntClauses->getNonemptyPushes(idx),
            sharedLearntClauses->getNonemptyGrabs(idx),
            sharedLearntClauses->getClausesPushed(idx),
            sharedLearntClauses->getClausesGrabbed(idx),
            sharedLearntClauses->getClausesFilteredOut(idx),
            static_cast<long long>(syncTime.count()),
            static_cast<long long>(lockWaitTime.count()));
  }

  fprintf(f, "cclausesharingstats %16s %16s %20s\n", "largest_push",
          "largest_grab", "most_clauses_in_bag");
  fprintf(f, "cclausesharingstats %16zu %16zu %20zu\n",
          sharedLearntClauses->getLargestPush(),
          sharedLearntClauses->getLargestGrab(),
          sharedLearntClauses->getMostClausesInBag());

  fprintf(f, "csolutionsharingstats %20s %20s %24s %24s\n", "nsols_pushed",
          "nsols_grabbed", "time_spent_syncing (ms)", "time_waiting_lock (ms)");

  for (size_t idx = 0; idx < _portfolio.size(); idx++) {
    auto syncTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        sharedSolutions->getSyncTime(idx));
    auto lockWaitTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        sharedSolutions->getLockWaitTime(idx));
    fprintf(f, "%-20s %20zu %20zu %24lld %24lld\n",
            ("solver" + std::to_string(idx)).c_str(),
            sharedSolutions->getSolutionsPushed(idx),
            sharedSolutions->getSolutionsPulled(idx),
            static_cast<long long>(syncTime.count()),
            static_cast<long long>(lockWaitTime.count())
);
  }
}

} // namespace openwbo
