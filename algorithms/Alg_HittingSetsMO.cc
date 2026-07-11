
// #define PARTIAL
#include <utility>
#ifndef PARTIAL

#include "Alg_HittingSetsMO.h"
#include <algorithm> // std::max
#include <iostream>

using namespace openwbo;
// using namespace NSPACE;
using NSPACE::toLit;

// this clones every variable in the solver. Make sure it is called
// accordingly
void HittingSetsMO::initializeOptimizer(Solver *solv, MaxSATFormula *mxf) {
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

void HittingSetsMO::genLowerBoundSet() { optim->searchAgain(); }

bool HittingSetsMO::diagnose(Solution::OneSolution &osol, vec<Lit> &assmpts) {
  diagnoses.emplace_back();
  auto &conflict = (--diagnoses.end())->clause();
  for (int i = 0, n = solver->conflict.size(); i < n; i++) {
    // i.e, diagnoses clause satisfied iff conflict is hit
    Lit lit = solver->conflict[i];
    conflict.push_back(lit);
    assmpts.remove(~lit);
  }
  return true;
}

bool HittingSetsMO::absorb(Solution::OneSolution &osol, int bvar) {
  {
    auto m = Model{osol.model()};
    // removes elements of solution that are dominated by m.
    if (solution().pushSafe(m, bvar, true, true)) {
      auto runtime = cpuTime();
      if (timestats[_time1stSol_] < 0) {
        timestats[_time1stSol_] = cpuTime() - initialTime;
        runstats[_nsatcalls1stSol_] = nbSatisfiable;
      }
      auto yp = solution().yPoint();
      std::ostringstream oss;
      oss << yp;
      std::osyncstream(std::cout)
          << getSolverId() << "c o " << oss.str() << "\n";
      // cout << getSolverId() << "c o " << yp << endl;
      runtime = cpuTime();
      printf("%sc new optimal solution (time: %.3f)\n", getSolverId().c_str(),
             runtime - initialTime);
    }
  }
  return true;
}

// saves sat lower bound set
bool HittingSetsMO::recycleLowerBoundSet() {
  const int nVars = getFormula()->nInitialVars();

  vec<Lit> assmpts{getFormula()->nInitialVars()};
  vec<Lit> partial_assmpts{};
  bool andf = true;
  for (auto &el : optim->solution()) {
    auto &osol = el.second.first;
    int id = el.first;
    Solution::notes_t bvar = el.second.second;
    // checks satisfiability of complete model
    modelClause(modelEmbed(osol.model(), nVars), assmpts);
    nbSatCalls++;

    auto conflicts_before = solver->conflicts;
    DLOG(LogCategory::SatCalls, stdout,
         "%sc sat_call_begin call=%d assumptions=%d budget_left=%d "
         "conflicts_before=%lu\n",
         getSolverId().c_str(), nbSatCalls, assmpts.size(), nConflicts,
         solver->conflicts);

    auto start = std::chrono::steady_clock::now();
    lbool sat = solver->solveLimited(assmpts);
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
         solver->conflicts - conflicts_before, solver->conflicts, nConflicts);

    if (sat == l_True) {
      nbSatisfiable++;
      absorb(osol, bvar);
    } else if (sat == l_False) {
      andf = false;
      std::ostringstream oss;
      oss << osol;
      std::osyncstream(std::cout) << getSolverId() << "c solution " << oss.str()
                                  << " not satisfiable\n";
      optim->mark_solution(id);
      diagnose(osol, assmpts);
    } else {
      return false;
    }
  }
  return andf;
}

void HittingSetsMO::incrementFormula() {
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
bool HittingSetsMO::setup_approx() {
  if (!diagnoses.size())
    return false;
  incrementFormula();
  diagnoses.clear();
  optim->increment();
  return optim->not_done();
}

bool HittingSetsMO::incorporate_approx() {
  consolidateSolution();
  return true;
}

void HittingSetsMO::consolidateSolution() {
  recycleLowerBoundSet();
  PBtoCNF::consolidateSolution();
}

void HittingSetsMO::vectorVec(const std::vector<Lit> &vector, vec<Lit> &vec) {
  for (int i = 0, n = vec.size(); i < n; i++)
    vec[i] = vector[i];
}
void HittingSetsMO::build() {
  PBtoCNF::build();
  MaxSATFormula *f = new MaxSATFormula{};
  initializeOptimizer(NULL, f);
}
bool HittingSetsMO::buildWorkFormula() { return optim->buildWorkFormula(); }

#endif
#undef PARTIAL
