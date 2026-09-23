// #define PARTIAL
#include <utility>
#ifndef PARTIAL

#include "Alg_ParHittingSetsMO.h"
#include <algorithm> // std::max
#include <iostream>

using namespace openwbo;
// using namespace NSPACE;
using NSPACE::toLit;

void ParHittingSetsMO::initializeOptimizer(Solver *solv, MaxSATFormula *mxf) {
  auto &f = *mxf;
  for (int i = 0, n = getFormula()->nObjFunctions(); i < n; ++i) {
    auto pb = getFormula()->getObjFunction(i);
    f.addObjFunction(pb);
  }
  for (int i = 0, n = getFormula()->nInitialVars(); i < n; ++i)
    f.newVar();
  {
    f.setInitialVars(f.nVars());
    optim->loadFormula(&f);
    optim->ConflictLimit(conflict_limit);
    optim->build();
    auto formula = optim->getFormula();
    int64_t min = 0, max = 0;
    for (int i = 0; i < formula->nObjFunctions(); i++) {
      max = formula->getObjFunction(i)->ub();
      formula->setUB(i, max);
      formula->setTighterUB(i, max);
      min = formula->getObjFunction(i)->lb();
      formula->setLB(i, min);
      formula->setTighterLB(i, min);
    };
    formula->setFormat(_FORMAT_PB_);
  }
}

void ParHittingSetsMO::genLowerBoundSet() { optim->searchAgain(); }

bool ParHittingSetsMO::diagnose(const std::vector<vec<Lit>> &unsatCores) {
#pragma omp critical(diagnoses)
  {
    for (const auto &core : unsatCores) {
      diagnosis diag;

      for (int i = 0; i < core.size(); i++)
        diag.clause().push_back(core[i]);

      diagnoses.push_back(std::move(diag));
    }
  }
  return !unsatCores.empty();
}

bool ParHittingSetsMO::absorb(size_t wid, CandidateSolution &csol) {
  {
    Worker &w = workers[wid];
    auto m = csol.model;
    auto bvar = csol.bvar;
    // removes elements of solution that are dominated by m.
    if (w.solutions.pushSafe(m, bvar, true, true)) {
      sharedSolutions->syncSolutions({w.solutions.oneSolution()}, wid, false);
      auto runtime = cpuTime();
#pragma omp critical(runtimestats)
      {
        if (timestats[_time1stSol_] < 0) {
          timestats[_time1stSol_] = cpuTime() - initialTime;
          runstats[_nsatcalls1stSol_] = nbSatisfiable;
        }
      }
      auto yp = w.solutions.yPoint();
      std::ostringstream oss;
      oss << yp;
      std::osyncstream(std::cout)
          << getSolverId() << "c o " << oss.str() << "\n";
      runtime = cpuTime();
      printf("%sc new optimal solution (time: %.3f)\n", getSolverId().c_str(),
             runtime - initialTime);
    }
  }
  return true;
}

// saves sat lower bound set
bool ParHittingSetsMO::recycleLowerBoundSet() {
  const int nVars = getFormula()->nInitialVars();
  std::vector<CandidateSolution> candidates{};

  for (auto &el : optim->solution()) {
    auto &osol = el.second.first;
    int id = el.first;
    Solution::notes_t bvar = el.second.second;
    candidates.push_back({id, osol.model(), bvar});
  }

  bool andf = true;

#pragma omp parallel num_threads(workers.size()) reduction(&& : andf)
  {
    size_t wid = omp_get_thread_num();
    workers[wid].assumptions.clear();
    Worker &w = workers[wid];

    std::vector<int> rejectedIDs{};
    std::vector<vec<Lit>> unsatCores{};

#pragma omp for nowait
    for (size_t i = 0; i < candidates.size(); i++) {
      int id = candidates[i].id;
      Model model = candidates[i].model;

      modelClause(modelEmbed(model, nVars), w.assumptions);

      auto conflicts_before = w.solver->conflicts;
      DLOG(LogCategory::SatCalls, stdout,
           "%sc sat_call_begin call=%d assumptions=%d budget_left=%d "
           "conflicts_before=%lu\n",
           getSolverId().c_str(), nbSatCalls, w.assumptions.size(),
           w.nConflicts, workers[wid].solver->conflicts);

      auto start = std::chrono::steady_clock::now();
      lbool sat;
      do {
        sat = solve(wid);
        if (sat == l_Undef) {
          DLOG(LogCategory::SatCalls, stdout,
               "%sc feasibility budget exhausted worker=%zu candidate=%zu; "
               "retrying\n",
               getSolverId().c_str(), wid, i);
          if (getShareClauses())
            shareClauses(wid);
        }
      } while (sat == l_Undef);
      auto end = std::chrono::steady_clock::now();
      double elapsed_ms =
          std::chrono::duration<double, std::milli>(end - start).count();

      auto res_str = (sat == l_True)    ? "SAT"
                     : (sat == l_False) ? "UNSAT"
                                        : "UNDEF";

      DLOG(LogCategory::SatCalls, stdout,
           "%sc sat_call_end call=%d result=%s time_ms=%.3f "
           "delta_conflicts=%lu conflicts_after=%lu budget_left=%d\n",
           getSolverId().c_str(), nbSatCalls, res_str, elapsed_ms,
           w.solver->conflicts - conflicts_before, w.solver->conflicts,
           w.nConflicts);

      if (sat == l_True) {
        absorb(wid, candidates[i]);
      } else if (sat == l_False) {
        andf = false;
        YPoint yp = evalModel(modelEmbed(candidates[i].model, nVars));
        std::ostringstream oss;
        oss << yp;
        std::osyncstream(std::cout) << getSolverId() << "c solution "
                                    << oss.str() << " not satisfiable\n";

        rejectedIDs.push_back(id);
        unsatCores.emplace_back();
        w.solver->conflict.copyTo(unsatCores.back());
      } else {
        andf = false;
      }

      // Share learnt clauses between the parallel feasibility workers.
      if (getShareClauses())
        shareClauses(wid);
    }
#pragma omp critical(optim)
    for (auto &id : rejectedIDs)
      optim->mark_solution(id);

    diagnose(unsatCores);
  }

  for (auto &worker : workers) {
    for (const auto &[id, entry] : worker.solutions)
      solution().pushSafe(entry.first.model(), entry.second, true, true);

    worker.solutions.clear();
  }

  return andf;
}

void ParHittingSetsMO::incrementFormula() {
  std::osyncstream(std::cout)
      << getSolverId() << "diagnoses size: " << diagnoses.size() << "\n";
  set<Lit> slice;

  for (auto &diag : diagnoses) {
    for (auto &el : diag.second)
      slice.insert(el);
    vec<Lit> vecDiag(diag.clause().size());
    vectorVec(diag.clause(), vecDiag);
    optim->getSolver()->addClause(vecDiag);
  }

  optim_sliced->thaw(slice);
  optim->checkSols();
}
bool ParHittingSetsMO::setup_approx() {
  if (!diagnoses.size())
    return false;
  incrementFormula();
  diagnoses.clear();
  optim->increment();
  return optim->not_done();
}

bool ParHittingSetsMO::incorporate_approx() {
  consolidateSolution();
  return true;
}

void ParHittingSetsMO::consolidateSolution() {
  recycleLowerBoundSet();
}

void ParHittingSetsMO::search_MO() {
  build();
  buildWorkFormula();

  StatusCode res = _UNKNOWN_;
  do {
    res = optim->searchAgain();

    incorporate_approx();

    if (res == _BUDGET_) {
      answerType = res;
      break;
    }

  } while (setup_approx());

  if (res == _OPTIMUM_ || res == _UNSATISFIABLE_) {
    answerType = solution().size() == 0 ? _UNSATISFIABLE_ : _OPTIMUM_;
  } else if (answerType != _BUDGET_) {
    answerType = res;
  }

  printAnswer(answerType);
}

void ParHittingSetsMO::vectorVec(const std::vector<Lit> &vector,
                                 vec<Lit> &vec) {
  for (int i = 0, n = vec.size(); i < n; i++)
    vec[i] = vector[i];
}
void ParHittingSetsMO::build() {
  init();
  initWorkers();
  buildSolversMO();

  auto *f = new MaxSATFormula{};
  initializeOptimizer(nullptr, f);
}
bool ParHittingSetsMO::buildWorkFormula() { return optim->buildWorkFormula(); }

#endif
#undef PARTIAL
