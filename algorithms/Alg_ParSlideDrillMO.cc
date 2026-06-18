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
    blockDominatedRegion(omp_get_thread_num(), workers[0].first.yPoint());
    YPoint yp{};
    for (int i = 0, n = getFormula()->nObjFunctions(); i < n; i++)
      yp.push_back(getFormula()->getUB(i) - getFormula()->getLB(i));
    waiting_list->insert(yp);
    searchBoundHonerMO();
    consolidateSolution();
    answerType = _OPTIMUM_;
  } else
    answerType = openwbo::_UNSATISFIABLE_;
  printAnswer(answerType);
}
bool ParSlideDrillMO::searchBoundHonerMO() {
  // drill
  while (answerType != _BUDGET_) {
    // if (!drill())
    break;
    // if (slide()) {
    bool conflicts_empty;
#pragma omp parallel reduction(&& : conflicts_empty)
    conflicts_empty =
        workers[omp_get_thread_num()].solver->conflict.size() == 0;

    if (conflicts_empty)
      return true;
  }
  // }
  return true;
}
// focus region dominating drill point, through manipulation of assumptions
bool ParSlideDrillMO::drill(size_t wid) {
  Worker &w = workers[wid];
  SDWorkerState &ws = worker_states[wid];
  lbool sat{l_False};
  YPoint yp{};
  while (waiting_list->size()) { // TODO: sync the waiting_list
    w.assumptions.clear();
    yp = waiting_list->pop(); // TODO: sync the waiting_list

    std::ostringstream oss;
    oss << "[" << wid << "] c drill from " << yp << " with hv=" << hv(yp)
        << "\n";
    std::osyncstream(std::cout) << oss.str();
    auto it = ws.mem.find(yp);
    // assume dominating region, until the next drill takes place.
    assumeDominatingRegion(wid, yp);
    if (it != ws.mem.end()) {
      ws.drill_marker = yp;
      for (auto l : it->second.deps)
        w.assumptions.push(~l);
    }

    // look for the first queued element that is not optimal
    if ((sat = solve(wid)) != l_False)
      break;
    else {
      fprintf(stdout, "[%zu] c if sat, optimal solution (time: %.3f)\n", wid,
              runtime - initialTime);
      if (!w.solver->conflict.size())
        return false;
      oss.str("");
      oss.clear();
      oss << "[" << wid << "] c o " << yp << "\n";
    }
  }
  w.assumptions.clear();
  if (sat == l_Undef) {
    std::ostringstream oss;
    oss << "[" << wid << "] c budget exhausted during drill. Push " << yp
        << " again\n";
    std::osyncstream(std::cout) << oss.str();
    answerType = _BUDGET_;
    waiting_list->unpop(1);
    return false;
  }
  if (sat == l_False)
    return false;
  ws.drill_marker = yp;
  return true;
}

// temporarily disable region dominating yp, so that the solver will slide
bool ParSlideDrillMO::slide(size_t wid) {
  //   lbool sat{};
  //   auto n_it = mem.find(drill_marker);
  //   PBtoCNF::assumeDominatingRegion(drill_marker);
  //   if (n_it != mem.end()) {
  //     std::cout << "c restart slide under " << drill_marker << endl;
  //     // block region below slide produces
  //     for (auto dep : n_it->second.deps)
  //       assumptions.push(~dep);
  //   } else {
  //     Node n{drill_marker};
  //     auto pair = mem.insert({drill_marker, n});
  //     n_it = pair.first;
  //   }
  //   Node &n{n_it->second};
  //   do {
  //     Model m = make_model(solver->model);
  //     // Only block dominated region if m1 gets into the Solution
  //     if (solution().pushSafe(m)) {
  //       // block region dominated by last point
  //       auto sol = solution().oneSolution();
  //       auto yp = sol.yPoint();
  //       // create slide variable for newly found solution
  //       auto l = mkLit(solver->newVar());
  //       n.deps.push_back(l);
  //       slide_map[l] = yp;
  //       waiting_list->insert(yp);
  //       printf("c o ");
  //       std::cout << sol << std::endl;
  //       runtime = cpuTime();
  //       printf("c new suboptimal solution (time: %.3f)\n", runtime -
  //       initialTime);
  //       // temporarily avoid region dominating last point
  //       std::cout << "c " << "slide from " << yp << endl;
  //       blockStep(yp);
  //       // add temporary clause, and set toggling variable through global
  //       // assumptions
  //       asssumeIncomparableRegion(yp, l);
  //       assumptions.push(~l);
  //     }
  //   } while ((sat = solve()) == l_True);
  //   // fix temporary variables used during slide, which are listed
  //   // in the assumptions.
  //   if (sat == l_Undef) {
  //     std::cout << "c budget exhausted during slide.";
  //     if (!drill_marker.empty()) {
  //       std::cout << " Push " << drill_marker << " again";
  //       waiting_list->unpop(0);
  //     }
  //     std::cout << endl;
  //     answerType = _BUDGET_;
  //     return false;
  //   }
  //   for (auto l : n.deps) {
  //     solver->addClause(l);
  //   }
  //   mem.erase(drill_marker);
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

} // namespace openwbo
