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

#pragma omp parallel num_threads(workers.size())
  {
    const size_t wid = omp_get_thread_num();
    auto &localList = *worker_states[wid].local_list;

    while (true) {
      std::optional<YPoint> point;
      bool finished = false;

#pragma omp critical(work_state)
      {
        point = localList.try_pop();

        if (!point)
          point = waiting_list->try_pop();

        if (point) {
          active.fetch_add(1);
        } else if (active.load() == 0) {
          bool allLocalEmpty = true;

          for (const auto &ws : worker_states) {
            if (ws.local_list->size() > 0) {
              allLocalEmpty = false;
              break;
            }
          }
          if (allLocalEmpty && waiting_list->size() == 0) {
            finished = true;
          }
        }
      }
      if (finished)
        break;

      if (!point)
        continue;

      // A point is published only after its solution is shared. Import all
      // newly published dominated-region blocks before processing that point.
      shareSolutions(wid, true);

      DrillResult result = drillFromPoint(wid, *point);
      bool requeueLocally = false;

      if (result == DrillResult::FoundModel) {
        requeueLocally = slide(wid) == SlideResult::Budget;
      } else if (result == DrillResult::UnsatCore) {
        // Skip
      } else if (result == DrillResult::UnsatNoCore) {

      } else if (result == DrillResult::Budget) {
        requeueLocally = true;
      }

#pragma omp critical(work_state)
      {
        if (requeueLocally)
          localList.requeue(*point);

        active.fetch_sub(1);
      }
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

  auto it = ws.mem.find(yp);
  assumeDominatingRegion(wid, yp);
  if (it != ws.mem.end()) {
    ws.drill_marker = yp;
    for (auto l : it->second.deps)
      w.assumptions.push(~l);
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
SlideResult ParSlideDrillMO::slide(size_t wid) {
  auto &w = workers[wid];
  auto &ws = worker_states[wid];
  Node *n = nullptr;
  lbool sat{};
  assumeDominatingRegion(wid, ws.drill_marker);
  auto n_it = ws.mem.find(ws.drill_marker);
  if (n_it != ws.mem.end()) {
    std::ostringstream oss;
    oss << ws.drill_marker;
    std::osyncstream(std::cout)
        << getSolverId() << "c restart slide under " << oss.str() << endl;
    // block region below slide produces
    for (auto dep : n_it->second.deps)
      w.assumptions.push(~dep);
  } else {
    Node n{ws.drill_marker};
    auto pair = ws.mem.insert({ws.drill_marker, n});
    n_it = pair.first;
  }
  n = &n_it->second;

  std::vector<YPoint> new_solutions_batch;

  do {
    Model m = make_model(w.solver->model);
    // Only block dominated region if m1 gets into the Solution
    if (w.solutions.pushSafe(m)) {
      // block region dominated by last point
      YPoint yp = evalModel(m);
      // create slide variable for newly found solution
      Lit l = mkLit(w.solver->newVar());
      n->deps.push_back(l);
      ws.slide_map[l] = yp;

      // add new solution to the batch, which will be added to the waiting list
      // after the slide is complete
      new_solutions_batch.push_back(yp);

      std::ostringstream oss;
      oss << yp;
      std::osyncstream(std::cout)
          << getSolverId() << "c o " << oss.str() << "\n";
      runtime = cpuTime();
      printf("%sc new suboptimal solution (time: %.3f)\n",
             getSolverId().c_str(), runtime - initialTime);
      // temporarily avoid region dominating last point
      std::osyncstream(std::cout)
          << getSolverId() << "c " << "slide from " << oss.str() << endl;
      blockStep(wid, yp); // block region dominated by yp
      // add temporary clause, and set toggling variable through global
      // assumptions
      asssumeIncomparableRegion(wid, yp, l);
      w.assumptions.push(~l);
      shareClauses(wid);
    }
  } while ((sat = solve(wid)) == l_True);

  // If the slide is successful (i.e. complete), add the temporary clauses to
  // the solver
  if (sat != l_Undef)
    for (Lit l : n->deps) {
      w.solver->addClause(l);
    }

  shareSolutions(wid, true);

#pragma omp critical(work_state)
  {
    for (const auto &yp : new_solutions_batch)
      waiting_list->insert(yp);
  }

  // fix temporary variables used during slide, which are listed
  // in the assumptions.
  if (sat == l_Undef) {
    std::osyncstream(std::cout)
        << getSolverId() << "c budget exhausted during slide.\n";
    std::ostringstream oss;
    oss << ws.drill_marker;
    if (!ws.drill_marker.empty()) {
      std::osyncstream(std::cout)
          << getSolverId() << " Push " << oss.str() << " again";
    }
    std::cout << endl;
    // answerType = _BUDGET_;
    return SlideResult::Budget;
  }

  ws.mem.erase(ws.drill_marker);
  return SlideResult::Done;
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
    worker_states[wid].local_list =
        waiting_list::construct(static_cast<int>(wl_type), lower, ascend);
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

void ParSlideDrillServerMO::bootstrap(const Solution &sol) {
  if (sol.size() == 0)
    return;

  const Model &m = sol.cbegin()->second.first.model();
  YPoint yp = evalModel(m);

  for (size_t wid = 0; wid < workers.size(); wid++) {
    blockDominatedRegion(wid, yp);
  }
}

} // namespace openwbo
