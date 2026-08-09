#include "Alg_ConflictShuntMO.h"
#include "Alg_HittingSetsServerMO.h"

bool ConflictShuntMO::setup_approx() {
  if (lower.get() && upper.get())
    optim = (optim == upper.get()) ? lower.get() : upper.get();
  optim->increment();
  return optim->not_done();
}
bool ConflictShuntMO::incorporate_approx() {
  while (optim->solution().size()) {
    auto osol = optim->solution().oneSolution();
    auto bvar = optim->solution().note();
    optim->solution().pop();
    auto m = Model{osol.model()};
    if (solution().pushSafe(m, bvar, true, true)) {
      if (bvar > 0)
        blockStep(solution().yPoint());
    }
  }
  return false;
}
void ConflictShuntMO::initializeOptimizer(Solver *s, MaxSATFormula *m) {
  if (upper != NULL) {
    upper->setFormula(getFormula());
    upper->setSolver(s);
    upper->setRootLits(objRootLits, invObjRootLits);
    upper->ConflictLimit(conflict_limit);
    upper->build();
    upper->setClauseSharingVarCutoff(clause_sharing_var_cutoff);
  }
  if (lower != NULL) {
    lower->setFormula(getFormula());
    lower->setSolver(s);
    lower->ConflictLimit(conflict_limit);
    lower->setRootLits(objRootLits, invObjRootLits);
    lower->build();
    lower->setClauseSharingVarCutoff(clause_sharing_var_cutoff);
  }
}
void ConflictShuntMO::build() {
  PBtoCNF::build();
  initializeOptimizer(solver, getMaxSATFormula());
}
bool ConflictShuntMO::buildWorkFormula() {
  PBtoCNFMasterMO::buildWorkFormula();
  // if(lower.get() != NULL){
  //   lower->buildWorkFormula();
  //   return true;
  // }
  // if(upper.get() != NULL) upper->buildWorkFormula();
  return true;
}
void ConflictShuntMO::search_MO() {
  build();
  if (firstSolution()) {
    buildWorkFormula();
    blockDominatedRegion(first.yPoint());
    auto res = searchConflictShuntMO();
    if (getStopSearchFlag()) {
      answerType = _INTERRUPTED_;
      DLOG(LogCategory::SatCalls, stdout,
           "%sstopSearch has been set to true, another thread requested to "
           "stop the search. Search stopped.\n",
           getSolverId().c_str());
      return;
    }
    PBtoCNF::consolidateSolution();
    if (res == _OPTIMUM_ || res == _UNSATISFIABLE_)
      if (solution().size() == 0 && !hasSharedSolutions())
        answerType = _UNSATISFIABLE_;
      else
        answerType = _OPTIMUM_;
    else
      answerType = res;

  } else
    answerType = openwbo::_UNSATISFIABLE_;
  shareSolutions(getShareSolutions());
  requestStopSearch();
  if (!isInsidePortfolio() || (!getShareSolutions() && !getShareClauses()))
    printAnswer(answerType);
}
StatusCode ConflictShuntMO::searchConflictShuntMO() {
  auto res = _UNKNOWN_;
  auto seededOptim = optim;
  auto initialNbSatCalls = nbSatCalls;
  auto initialNbSatisfiable = nbSatisfiable;

  if (optim) {
    optim->setNbSatCalls(nbSatCalls);
    optim->setNbSatisfiable(nbSatisfiable);
  }

  do {
    res = compute_approx();
    if (getStopSearchFlag()) {
      answerType = _INTERRUPTED_;
      DLOG(LogCategory::SatCalls, stdout,
           "%sstopSearch has been set to true, another thread requested to "
           "stop the search. Search stopped.\n",
           getSolverId().c_str());
      return answerType;
    }
    incorporate_approx();
    shareClauses();
    shareSolutions(getShareSolutions());
    if (res != _BUDGET_)
      break;
  } while (setup_approx());
  nbSatCalls = initialNbSatCalls;
  nbSatisfiable = initialNbSatisfiable;
  auto addServerStats = [&](const shared_ptr<PBtoCNFServerMO> &server) {
    if (!server)
      return;
    auto serverNbSatCalls = server->getNbSatCalls();
    auto serverNbSatisfiable = server->getNbSatisfiable();
    if (server.get() == seededOptim) {
      serverNbSatCalls -= initialNbSatCalls;
      serverNbSatisfiable -= initialNbSatisfiable;
    }
    nbSatCalls += serverNbSatCalls;
    nbSatisfiable += serverNbSatisfiable;
  };
  addServerStats(upper);
  addServerStats(lower);
  return res;
}
void ConflictShuntMO::consolidateSolution() {
  incorporate_approx();
  PBtoCNF::consolidateSolution();
}
void ConflictShuntMO::printAnswer(int answerType) {
  if (lower)
    lower->printAnswer(0);
  if (upper)
    upper->printAnswer(0);
  solution().report();
  MOCO::printAnswer(answerType);
}

bool ParConflictShuntMO::setup_approx() {
  if (lower.get() && upper.get())
    optim = (optim == upper.get()) ? lower.get() : upper.get();
  optim->increment();
  return optim->not_done();
}

bool ParConflictShuntMO::incorporate_approx() {

  auto sols = optim->sharedSolutions->getSolutions();
  while (sols.size()) {
    openwbo::Solution::OneSolution osol = sols.back().second;
    sols.pop_back();
    auto m = Model{osol.model()};
    if (solution().pushSafe(m, -1, true,
                            true)) { // TODO: correct once notes are added to
                                     // sharedSolutions if (osol.note() > 0)
      blockStep(MASTER_WORKER_ID, solution().yPoint());
    }
  }
  return false;
}

void ParConflictShuntMO::initializeOptimizer(Solver *s, MaxSATFormula *m) {
  if (upper != NULL) {
    upper->setFormula(getFormula());
    upper->initWorkers();
    setSharedSolutions(upper->getSharedSolutions());
    upper->buildSolversMO();

    std::atomic<bool> feasible{true};
#pragma omp parallel num_threads(upper->nWorkers())
    {
      const size_t wid = omp_get_thread_num();
      if (!upper->firstSolution(wid))
        feasible.store(false);

#pragma omp barrier

      if (feasible.load()) {
        upper->updateMOFormulation(wid);
        upper->blockDominatedRegion(wid,
                                    upper->workers[wid].first.yPoint());
      }
    }

    upper->setConflictLimit(conflict_limit);
  }
  // if (lower != NULL) {
  //   lower->setFormula(getFormula());
  //   lower->setSolver(s);
  //   lower->ConflictLimit(conflict_limit);
  //   lower->setRootLits(objRootLits, invObjRootLits);
  //   lower->build();
  //   lower->setClauseSharingVarCutoff(_nb_encoded_vars_initial);
  // }
}

void ParConflictShuntMO::build() {
  init();
  ParallelMO::initWorkers();
  nbMCS = 0;
  answerType = openwbo::_UNKNOWN_;
  if (!workers[0].solver)
    buildSolversMO();

  YPoint yp{};
  auto n = getFormula()->nObjFunctions();
  for (int i = 0; i < n; i++)
    yp.push_back(getFormula()->getUB(i) - getFormula()->getLB(i));
  pareto::max = yp;
  pareto::min = YPoint(n);
  pareto::hv_total = pareto::hv_shift(pareto::min, pareto::max);

  Solver *masterSolver = workers[0].solver;
  initializeOptimizer(masterSolver, getMaxSATFormula());
  masterSolver->setConfBudget(conflict_limit);
}

// will propagate changes innoculated by Master.
void HittingSetsConflictServerMO::increment() {
  if (answerType == openwbo::_BUDGET_)
    answerType = openwbo::_UNKNOWN_;
}

StatusCode ParConflictShuntMO::searchConflictShuntMO() {
  auto res = _UNKNOWN_;
  auto seededOptim = optim;
  auto initialNbSatCalls = nbSatCalls;
  auto initialNbSatisfiable = nbSatisfiable;

  // if (optim) {
  //   optim->setNbSatCalls(nbSatCalls);
  //   optim->setNbSatisfiable(nbSatisfiable);
  // }

  do {
    res = compute_approx();
    incorporate_approx();
    if (res != _BUDGET_)
      break;
  } while (setup_approx());
  nbSatCalls = initialNbSatCalls;
  nbSatisfiable = initialNbSatisfiable;
  // auto addServerStats = [&](const shared_ptr<PBtoCNFServerMO> &server) {
  //   if (!server)
  //     return;
  //   auto serverNbSatCalls = server->getNbSatCalls();
  //   auto serverNbSatisfiable = server->getNbSatisfiable();
  //   if (server.get() == seededOptim) {
  //     serverNbSatCalls -= initialNbSatCalls;
  //     serverNbSatisfiable -= initialNbSatisfiable;
  //   }
  //   nbSatCalls += serverNbSatCalls;
  //   nbSatisfiable += serverNbSatisfiable;
  // };
  // addServerStats(upper);
  // addServerStats(lower);
  return res;
}

void ParConflictShuntMO::search_MO() {
  build();
  if (firstSolution(MASTER_WORKER_ID)) {
    buildWorkFormula();
    blockDominatedRegion(MASTER_WORKER_ID,
                         workers[MASTER_WORKER_ID].first.yPoint());
    auto res = searchConflictShuntMO();
    ParallelMO::consolidateSolution(MASTER_WORKER_ID);
    if (res == _OPTIMUM_ || res == _UNSATISFIABLE_)
      if (sharedSolutions->empty())
        answerType = _UNSATISFIABLE_;
      else
        answerType = _OPTIMUM_;
    else
      answerType = res;

  } else
    answerType = openwbo::_UNSATISFIABLE_;
  printAnswer(answerType);
}

void ParConflictShuntMO::printAnswer(int answerType) {
  ParallelMO::printAnswer(answerType);
}

void ParConflictShuntMO::printStats() {
  if (upper)
    upper->printStats();
  else if (lower)
    lower->printStats();
  else
    ParallelMO::printStats();
}
