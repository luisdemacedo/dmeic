#include "Alg_PortfolioMO.h"

namespace openwbo {

StatusCode PortfolioMO::search() {
#pragma omp parallel for
  for (size_t idx = 0; idx < _portfolio.size(); idx++) {
    _portfolio[idx]->search();
  }

  for (size_t idx = 0; idx < _portfolio.size(); idx++) {
    DLOG(stdout, "Answer from solver %zu: %u\n", idx,
         _portfolio[idx]->answerType);
    if (_portfolio[idx]->answerType != _INTERRUPTED_) {
      this->answerType = _portfolio[idx]->answerType;
    }
  }

  DLOG(stdout, "Answer from portfolio: %u\n", this->answerType);

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
  if (!getShareSolutions() && !getShareClauses())
    return;
  std::unique_lock<std::mutex> lock(*printResultsLock);
  if (verbosity > 0 && print)
    printStats();
  printf("c ---------- OUTPUT ---------------\n");

  if (!print)
    return;

  switch (type) {
  case _SATISFIABLE_:
    printf("s SATISFIABLE\n");
    printSolutions();
    printStats();
    break;
  case _OPTIMUM_:
    printf("s OPTIMUM\n");
    printSolutions();
    // printApproxRatio(); TODO: implement printApproxRatio() for parallel MOCO
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
    printStats();
    break;
  }
}

void PortfolioMO::printSolutions() {
  std::ostream &f = std::cout;
  std::ofstream file;
  auto sols = sharedSolutions->getSolutions();
  if (print_model) {
    file.open(effsols_file);
    std::ostream &out =
        (print_my_output) ? static_cast<std::ostream &>(file) : std::cout;
    // Models
    for (auto &[t_id, sol] : sols) {
      Model m = sol.model();
      std::string line = "v [s" + std::to_string(t_id) + "]";
      for (int i = 0; i < maxsat_formula->nVars(); i++) {
        indexMap::const_iterator it = maxsat_formula->getIndexToName().find(i);
        if (it != maxsat_formula->getIndexToName().end()) {
          line += " ";
          if (m[i] == l_True)
            line += " ";
          else
            line += "-";
          line += it->second;
        }
      }
      std::osyncstream(out) << line << "\n";
    }
  }
  std::osyncstream(std::cout)
      << "c " << sols.size() << " (efficient) solutions" << "\n";
  std::cout << "c ------- " << std::endl;
  std::cout << "c pts of transformed prob" << std::endl;
  for (auto &[t_id, sol] : sols) {
    std::string line = "c [s" + std::to_string(t_id) + "] pt";
    for (int i = 0; i < maxsat_formula->nObjFunctions(); i++)
      line += " " + std::to_string(sol.yPoint()[i]);
    std::osyncstream(f) << line << "\n";
  }
  std::cout << "c ------- " << std::endl;
  std::osyncstream(std::cout) << "c " << sols.size() << " points T" << "\n";
  std::cout << "c ------- " << std::endl;
  std::cout << "c lower bound set of transformed prob" << std::endl;
  for (size_t i = 0; i < LBset.size(); i++) {
    std::string line = "c lb";
    for (int di = 0; di < maxsat_formula->nObjFunctions(); di++)
      line += " " + std::to_string(LBset[i][di]);
    std::osyncstream(f) << line << "\n";
  }
  std::cout << "c ------- " << std::endl;
  std::osyncstream(std::cout) << "c " << LBset.size() << " lbs T" << "\n";
  std::cout << "c ------- " << std::endl;

  // TODO: LBSet to file

  file.open(objv_file);
  std::ostream &out =
      (print_my_output) ? static_cast<std::ostream &>(file) : std::cout;

  for (auto &[t_id, sol] : sols) {
    std::string line = "o [s" + std::to_string(t_id) + "]";
    for (int i = 0; i < maxsat_formula->nObjFunctions(); i++)
      line += " " + std::to_string((int64_t)sol.yPoint()[i] +
                                   getFormula()->getObjFunction(i)->_const);
    std::osyncstream(out) << line << "\n";
  }
  std::osyncstream(std::cout)
      << "c " << sols.size() << " nondominated points" << "\n";
  std::string line = "c _consts:";
  for (size_t i = 0; i < getFormula()->nObjFunctions(); i++)
    line += " " + std::to_string(getFormula()->getObjFunction(i)->_const);
  std::osyncstream(std::cout) << line << "\n";
}

void PortfolioMO::updateStats() {
  for (size_t idx = 0; idx < _portfolio.size(); idx++) {
    int nev = 0, nec = 0, nerv = 0;
    _portfolio[idx]->runstats[_nencvars_] = 0;
    _portfolio[idx]->runstats[_nencclauses_] = 0;
    _portfolio[idx]->runstats[_nencrootvars_] = 0;
    _portfolio[idx]->runstats[_nreencodes_] = _portfolio[idx]->nreencodes;

    for (int i = 0; i < _portfolio[idx]->getFormula()->nObjFunctions(); i++) {

      if (_portfolio[idx]->enc_is_kp_based())
        _portfolio[idx]->kps[i].getEncodeSizes(&nev, &nec, &nerv);
      else if (_portfolio[idx]->encoder.getPBEncoding() == _PB_GTE_)
        _portfolio[idx]->gtes[i].getEncodeSizes(&nev, &nec, &nerv);
      _portfolio[idx]->runstats[_nencvars_] += nev;
      _portfolio[idx]->runstats[_nencclauses_] += nec;
      _portfolio[idx]->runstats[_nencrootvars_] += nerv;
      fprintf(stdout, "c objstats [s%d] %3d %5d %8d %9d\n", idx, i + 1, nev,
              nec, nerv);
    }
  }
}
void PortfolioMO::printStats() {
  double totalTime = cpuTime();

  FILE *f = stdout;

  std::cout << "c ------- " << std::endl;
  fprintf(f, "c lobjstats     obj nvars nclauses nrootvars\n");
  updateStats();

  std::cout << "c ------- " << std::endl;
  fprintf(f,
          "clrunstats     %18s %12s %12s %12s %12s %12s %18s %18s %18s %20s "
          "%18s %8s %8s\n",
          "nsatcalls_1stSol", "nsatcalls", "ncalls", "n_eff_sols", "n_nondom",
          "n_prob_vars", "n_prob_clauses", "n_enc_vars(sum)",
          "n_enc_clauses(sum)", "n_enc_rootvars(sum)", "n_reencodes", "rapprox",
          "nobj");
  for (size_t idx = 0; idx < _portfolio.size(); idx++) {
    fprintf(f,
            "crunstats %4s %18d %12d %12d %12d %12d %12d %18d %18d %18d %20d "
            "%18d %8.4f %8d\n",
            std::format("[s{}]", idx).c_str(),
            _portfolio[idx]->runstats[_nsatcalls1stSol_],
            _portfolio[idx]->nbSatisfiable, _portfolio[idx]->nbSatCalls,
            _portfolio[idx]->solution().size(),
            _portfolio[idx]->solution().size(),
            _portfolio[idx]->getFormula()->nVars(),
            _portfolio[idx]->getFormula()->nHard(),
            _portfolio[idx]->runstats[_nencvars_],
            _portfolio[idx]->runstats[_nencclauses_],
            _portfolio[idx]->runstats[_nencrootvars_],
            _portfolio[idx]->runstats[_nreencodes_], _portfolio[idx]->repsilon,
            _portfolio[idx]->getFormula()->nObjFunctions());
  }

  // if (getShareClauses()) {
  std::cout << "c ------- " << std::endl;
  fprintf(f,
          "clclausesharingstats       %20s %20s %20s %20s %20s %22s %24s %24s "
          "%16s %16s %20s\n",
          "nsynccalls", "nnonempty_pushes", "nnonempty_grabs",
          "nclauses_pushed", "nclauses_grabbed", "nclauses_filtered_out",
          "time_spent_syncing (ms)", "time_waiting_lock (ms)", "largest_push",
          "largest_pull", "most_clauses_in_bag");
  for (size_t idx = 0; idx < sharedLearntClauses->getNumWorkers(); idx++) {
    auto syncTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        sharedLearntClauses->getSyncTime(idx));
    auto lockWaitTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        sharedLearntClauses->getLockWaitTime(idx));

    fprintf(f,
            "%-21s %4s %20zu %20zu %20zu %20zu %20zu %22zu %24lld "
            "%24lld %16zu %16zu %20zu\n",
            "cclausesharingstats", std::format("[s{}]", idx).c_str(),
            sharedLearntClauses->getSyncs(idx),
            sharedLearntClauses->getNonemptyPushes(idx),
            sharedLearntClauses->getNonemptyGrabs(idx),
            sharedLearntClauses->getClausesPushed(idx),
            sharedLearntClauses->getClausesGrabbed(idx),
            sharedLearntClauses->getClausesFilteredOut(idx),
            static_cast<long long>(syncTime.count()),
            static_cast<long long>(lockWaitTime.count()),
            sharedLearntClauses->getLargestPush(),
            sharedLearntClauses->getLargestGrab(),
            sharedLearntClauses->getMostClausesInBag());
  }
  // }

  std::cout << "c ------- " << std::endl;
  fprintf(f, "clsolutionsharingstats      %20s %20s %24s %24s %20s %20s\n",
          "nsols_pushed", "nsols_grabbed", "time_spent_syncing (ms)",
          "time_waiting_lock (ms)", "time_pulling (ms)", "time_pushing (ms)");

  for (size_t idx = 0; idx < sharedSolutions->getNumWorkers(); idx++) {
    auto syncTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        sharedSolutions->getSyncTime(idx));
    auto lockWaitTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        sharedSolutions->getLockWaitTime(idx));
    auto pullTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        sharedSolutions->getPullTime(idx));
    auto pushTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        sharedSolutions->getPushTime(idx));
    fprintf(f, "%-21s %4s %20zu %20zu %24lld %24lld %20lld %20lld\n",
            "csolutionsharingstats", std::format("[s{}]", idx).c_str(),
            sharedSolutions->getSolutionsPushed(idx),
            sharedSolutions->getSolutionsPulled(idx),
            static_cast<long long>(syncTime.count()),
            static_cast<long long>(lockWaitTime.count()),
            static_cast<long long>(pullTime.count()),
            static_cast<long long>(pushTime.count()));
  }

  timestats[_totaltime_] = totalTime - initialTime;
  std::cout << "c ------- " << std::endl;
  fprintf(f, "cltimestats      %12s %12s\n", "time_1stSol", "totaltime");
  for (size_t idx = 0; idx < _portfolio.size(); idx++)
    fprintf(f, "ctimestats %4s %12.2f %12.2f\n",
            std::format("[s{}]", idx).c_str(),
            _portfolio[idx]->timestats[_time1stSol_],
            totalTime - _portfolio[idx]->initialTime);
}

} // namespace openwbo
