#include "Alg_ParSlideDrillMO.h"

namespace openwbo {
void ParSlideDrillMO::search_MO() {
  init();
  buildSolversMO();
  worker_states.resize(workers.size());
  size_t main_solver = 0;
  answerType = _UNSATISFIABLE_;
  if (firstSolution(main_solver)) {
#pragma omp parallel
    updateMOFormulation(omp_get_thread_num());

#pragma omp parallel
    blockDominatedRegion(omp_get_thread_num(),
                         workers[main_solver].first.yPoint());
    YPoint yp{};
    for (int i = 0, n = getFormula()->nObjFunctions(); i < n; i++)
      yp.push_back(getFormula()->getUB(i) - getFormula()->getLB(i));
    waiting_list->insert(yp);
    searchBoundHonerMO();
#pragma omp parallel
    consolidateSolution(omp_get_thread_num());
    answerType = openwbo::_OPTIMUM_;
  } else
    answerType = openwbo::_UNSATISFIABLE_;
  printAnswer(answerType);
}
bool ParSlideDrillMO::searchBoundHonerMO() {
  // drill
  std::atomic<int> active{0};
  std::atomic<bool> stop{false};
  printf("c ParSlideDrillMO::searchBoundHonerMO: start drilling\n");
#pragma omp parallel num_threads(workers.size())
  {
    size_t wid = omp_get_thread_num();
    while (!stop.load()) {

      std::optional<YPoint> maybe_yp = std::nullopt;
      maybe_yp = waiting_list->try_pop();
      if (!maybe_yp) {
        if (active.load() == 0 && waiting_list->size() == 0)
          break;

        continue;
      }
      active.fetch_add(1);
      DrillResult result = drillFromPoint(wid, *maybe_yp);

      if (result == DrillResult::FoundModel) {
        slide(wid);
      } else if (result == DrillResult::UnsatCore) {
        // Skip
      } else if (result == DrillResult::UnsatNoCore) {
        stop.store(true);
      } else if (result == DrillResult::Budget) {
        waiting_list->requeue(maybe_yp.value(), 1);
      }

      shareSolutions(wid, true);

      active.fetch_sub(1);
      printf("%sSolutions size: %zu\n", getSolverId().c_str(),
             sharedSolutions->getSolutions().size());
    }
  }
  answerType = _OPTIMUM_;
  return true;
}

DrillResult ParSlideDrillMO::drillFromPoint(size_t wid, const YPoint &yp) {
  Worker &w = workers[wid];
  SDWorkerState &ws = worker_states[wid];
  lbool sat{l_False};
  w.assumptions.clear();
  std::ostringstream oss;
  oss << getSolverId() << "c " << "drill from " << yp << " with hv=" << hv(yp)
      << "\n";
  std::osyncstream(std::cout) << oss.str();

#pragma omp critical(mem)
  {
    auto it = mem.find(yp);
    assumeDominatingRegion(wid, yp);
    if (it != mem.end()) {
      ws.drill_marker = yp;
      for (auto l : it->second.deps)
        w.assumptions.push(~l);
    }
  }

  oss.str("");
  oss.clear();

  sat = solve(wid);
  if (sat == l_False) {
    oss << getSolverId() << "c "
        << "if sat, optimal solution (time: " << runtime - initialTime << ")\n";
    std::osyncstream(std::cout) << oss.str();
    oss.str("");
    oss.clear();
    if (!w.solver->conflict.size())
      return DrillResult::UnsatNoCore;
    return DrillResult::UnsatCore;
  } else if (sat == l_Undef) {
    oss << getSolverId() << "c " << "budget exhausted during drill. Push " << yp
        << " again\n";
    std::osyncstream(std::cout) << oss.str();
    return DrillResult::Budget;
  }
  ws.drill_marker = yp;
  return DrillResult::FoundModel;
}

// temporarily disable region dominating yp, so that the solver will slide
bool ParSlideDrillMO::slide(size_t wid) {
  auto &w = workers[wid];
  auto &ws = worker_states[wid];
  Node *n = nullptr;
  lbool sat{};
  assumeDominatingRegion(wid, ws.drill_marker);
#pragma omp critical(mem)
  {
    auto n_it = mem.find(ws.drill_marker);
    if (n_it != mem.end()) {
      std::cout << "c restart slide under " << ws.drill_marker << endl;
      // block region below slide produces
      for (auto dep : n_it->second.deps)
        w.assumptions.push(~dep);
    } else {
      Node n{ws.drill_marker};
      auto pair = mem.insert({ws.drill_marker, n});
      n_it = pair.first;
    }
    n = &n_it->second;
  }
  do {
    Model m = make_model(w.solver->model);
    // Only block dominated region if m1 gets into the Solution
    if (w.solutions.pushSafe(m)) {
      // block region dominated by last point
      YPoint yp = evalModel(m);
      Lit l;
      // create slide variable for newly found solution
#pragma omp critical(mem)
      {

        std::vector<Lit> lits;
        lits.resize(workers.size());
        for (size_t i = 0; i < workers.size(); i++)
          lits[i] = mkLit(workers[i].solver->newVar());
        if (workers.size() > 1)
          assert(std::all_of(lits.begin(), lits.end(),
                             [&](Lit l) { return l.x == lits.front().x; }));
        l = lits[wid];
        n->deps.push_back(l);
        slide_map[l] = yp;
      }
      waiting_list->insert(yp);
      std::ostringstream oss;
      oss << yp;
      std::osyncstream(std::cout) << "c o " << oss.str() << "\n";
      runtime = cpuTime();
      printf("c new suboptimal solution (time: %.3f)\n", runtime - initialTime);
      // temporarily avoid region dominating last point
      std::cout << "c " << "slide from " << yp << endl;
      blockStep(wid, yp); // block region dominated by yp
      // add temporary clause, and set toggling variable through global
      // assumptions
      asssumeIncomparableRegion(wid, yp, l);
      w.assumptions.push(~l);
    }
  } while ((sat = solve(wid)) == l_True);

  // fix temporary variables used during slide, which are listed
  // in the assumptions.
  if (sat == l_Undef) {
    std::cout << "c budget exhausted during slide.";
    if (!ws.drill_marker.empty()) {
      std::cout << " Push " << ws.drill_marker << " again";
      waiting_list->requeue(ws.drill_marker, 1);
    }
    std::cout << endl;
    // answerType = _BUDGET_;
    return false;
  }
  for (auto l : n->deps) {
    w.solver->addClause(l);
  }
#pragma omp critical(mem)
  mem.erase(ws.drill_marker);
  return true;
}
void ParSlideDrillMO::asssumeIncomparableRegion(size_t wid, const YPoint &yp,
                                                Lit l) {
  Worker &w = workers[wid];
  auto nObj = yp.size();
  YPoint yp1 = yp;
  uint64_t objix[nObj];
  evalToIndex(wid, yp1, objix);
  vec<Lit> clause;
  for (YPoint::size_type di = 0; di < nObj; di++) {
    int j = objix[di];
    if (j < (int)(*w.objRootLits[di]).size()) {
      clause.push(~(*w.objRootLits[di]).at(j).second);
    }
  }
  clause.push(l);
  w.solver->addClause(clause);
}

void ParSlideDrillMO::initWorkers() {
  ParallelMO::initWorkers();
  worker_states.resize(workers.size());
  for (size_t wid = 0; wid < workers.size(); wid++) {
    worker_states[wid].drill_marker.clear();
  }
}

void ParSlideDrillServerMO::increment() {
  if (answerType == openwbo::_BUDGET_)
    answerType = openwbo::_UNKNOWN_;
}

StatusCode ParSlideDrillServerMO::searchAgain() {
  printf("c ParSlideDrillServerMO::searchAgain\n");
#pragma omp parallel for
  for (size_t wid = 0; wid < workers.size(); wid++)
    workers[wid].assumptions.clear();

  printf("c ParSlideDrillServerMO::searchAgain: clear assumptions\n");
  answerType = openwbo::_UNKNOWN_;
  searchBoundHonerMO();
  if (answerType != openwbo::_BUDGET_)
    if (sharedSolutions->empty())
      answerType = openwbo::_UNSATISFIABLE_;
    else
      answerType = openwbo::_OPTIMUM_;
  return answerType;
}

} // namespace openwbo
