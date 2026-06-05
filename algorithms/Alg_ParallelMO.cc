#include "Alg_ParallelMO.h"

using namespace openwbo;
using NSPACE::toLit;

void ParallelMO::init() { // Copied from PBtoCNF
  vec<int> vars;
  vars.growTo(getFormula()->nVars(), 0);

  if (!_useAllVars) {
    for (int i = 0; i < getFormula()->nSoft(); i++) {
      for (int j = 0; j < getFormula()->getSoftClause(i).clause.size(); j++) {
        int v = var(getFormula()->getSoftClause(i).clause[j]);
        assert(v < vars.size());
        if (vars[v] == 0) {
          _soft_variables.push(v);
          vars[v] = 1;
        }
      }
    }
  } else {
    for (int i = 0; i < getFormula()->nVars(); i++) {
      _soft_variables.push(i);
    }
  }
  _assigned_true.growTo(getFormula()->nInitialVars(), 0);
  _varScore.growTo(getFormula()->nInitialVars(), 0);

  _maxWeight = 0;
  for (int i = 0; i < getFormula()->nSoft(); i++) {
    Lit l = getFormula()->newLiteral();
    getFormula()->getSoftClause(i).relaxation_vars.push(l);
    getFormula()->getSoftClause(i).assumption_var =
        getFormula()->getSoftClause(i).relaxation_vars[0]; // Assumption
                                                           // Var is
                                                           // relaxation
                                                           // var

    _maxWeight += getFormula()->getSoftClause(i).weight;
  }
  printf("c Max. Weight: %ld\n", _maxWeight);
  // build the objRootLits vector
  for (auto &w : workers)
    for (int i = w.objRootLits.size(); i < getFormula()->nObjFunctions(); i++)
      w.objRootLits.push_back(
          std::make_shared<rootLits::RootLits>(rootLits::RootLits{}));
}

lbool ParallelMO::solve(size_t wid) {
  auto &w = workers[wid];
#pragma omp atomic
  nbSatCalls++;

  lbool res;
#ifdef SIMP
  res = ((SimpSolver *)solver)->solveLimited(assumptions[solver_id]);
#else
  if (conflict_limit < 0) {
    w.solver->budgetOff();
    res = w.solver->solveLimited(w.assumptions);
  }

  // signals the exhaustion of the budget. Reset the limit, and go on
  if (w.nConflicts < 0) {
    w.nConflicts = conflict_limit;
    return l_Undef;
  }

  auto old = w.nConflicts;
  w.solver->setConfBudget(w.nConflicts);
  w.nConflicts += w.solver->conflicts;
  res = w.solver->solveLimited(w.assumptions);
  if (res == l_True)
#pragma omp atomic
    nbSatisfiable++;

  w.nConflicts -= w.solver->conflicts;
  // ensure every unsat call spends at least one conflict:
  if (old == w.nConflicts && res == l_False)
    w.nConflicts--;
  // if undef, reset nConflicts
  if (res == l_Undef)
    w.nConflicts = conflict_limit;
#endif
  return res;
}

bool ParallelMO::firstSolution() {
  // if (first.model().size())
  //   return true;
  //
  // double before1stsol = cpuTime();
  // printf("c first call to solvers[0]\n");
  // int old = conflict_limit;
  // conflict_limit = -1;
  // lbool res = solve(omp_get_thread_num());
  //
  //
  return true;
}

bool ParallelMO::updateMOFormulationIfSAT(size_t wid) {
  printf("c [updateMOFormulationIfSAT]\n");
  //     solver->my_print();

  if (!firstSolution())
    return false;
  updateMOFormulation(wid);
  return true;
}

bool ParallelMO::updateMOFormulation(size_t wid) {
  Worker &w = workers[wid];
  printf("c [updateMOFormulation]\n");
  if (w.nreencodes == 0) {
    int nObj = getFormula()->nObjFunctions();
    // for (int di = 0; di < nObj; di++)
    // fubs[di] = getFormula()->getObjFunction(di)->ub();
  }

  double before_enc = cpuTime();
  updateMOEncoding(
      wid); // aqui é esquecido o encoding anterior, caso seja refeito
  double total_time = cpuTime();
  printf("c encode time: %f\n", total_time - before_enc);

  return true;
}

void ParallelMO::updateMOEncoding(size_t wid) {
  Worker &w = workers[wid];
  w.nreencodes++;
  if (enc_is_kp_based(wid) || w.encoder.getPBEncoding() == _PB_GTE_ ||
      w.encoder.getPBEncoding() == _PB_IGTE_) {
    printf("c updateMOEncoding\n");
    //         solver->my_print();
    //         assumptions.clear();
    for (int i = 0; i < getFormula()->nObjFunctions(); i++) {
      PBObjFunction pb{*getFormula()->getObjFunction(i)};
      auto factor = pb._factor;

      auto &ith_orl = *w.objRootLits[i].get();

      if (enc_is_kp_based(wid) || w.encoder.getPBEncoding() == _PB_GTE_) {

        if (w.objRootLits[i] && w.objRootLits[i]->size() > 0) {
          //                     objRootLits[i].clear();
          printf("c clear encoding of obj. funct. %d\n", i);
          if (enc_is_kp_based(wid)) {
            //                         kps[i].clearedEncoding(solver);
            // TODO
          }
        }

        printf("\nc encode (function %d upper bound: %lu)\n", i, w.fubs[i]);
        getFormula()->getObjFunction(i)->my_print(
            getFormula()->getIndexToName());

        encoding::wlit_mapt rootLits;
        if (!pb.empty()) {
          if (enc_is_kp_based(wid)) {
            w.kps[i].setApproxRatio(w.solver, 1, 0);
            // kps[i].setNameToIndex(getFormula()->getNameToIndex());
            w.kps[i].encode(w.solver, pb);
          } else if (w.encoder.getPBEncoding() == _PB_GTE_) {
            w.gtes[i].encode(w.solver, pb);
          }

          // printf("get root lits\n");

          // TODO: get root literals (sorted), save them, and create
          // order encoding printf("Root Literals (o.f. %d)\n", i+1);
          // printRootLit(i);
          if (enc_is_kp_based(wid))
            rootLits = w.kps[i].getRootLits();
          else if (w.encoder.getPBEncoding() == _PB_GTE_)
            rootLits = w.gtes[i].getRootLits();
        }
        getFormula()->replaceObjFunction(
            i, make_unique<PBObjFunction>(std::move(pb)));
        int o = 0;
        vec<Lit> clause;
        // order encoding
        if (enc_is_kp_based(i)) {
          ith_orl.clear();
          encoding::wlit_mapt::iterator prev;
          for (encoding::wlit_mapt::iterator rit = rootLits.begin();
               rit != rootLits.end(); rit++) {
            ith_orl.push(
                std::pair<uint64_t, Lit>(factor * rit->first + 1, rit->second));
            w.invObjRootLits->insert({var(rit->second), i});
          }

        } else {
          encoding::wlit_mapt::reverse_iterator prev;
          // se x_d = 1, entao a sol tera de ser <= d
          for (encoding::wlit_mapt::reverse_iterator rit = rootLits.rbegin();
               rit != rootLits.rend(); rit++) {
            // objRootLits[i].push_back(std::pair<uint64_t,
            // Lit>(rit->first, rit->second));
#ifdef __DEBUG__
            printf("\t[%lu] ", rit->first);
            if (sign(rit->second))
              printf("~");
            printf("y%d\n", var(rit->second) + 1);
#endif
            if (o > 0) {
              clause.push(~prev->second);
              clause.push(rit->second);
              w.solver->addClause(clause);
              clause.clear();
            }
            prev = rit;
            o++;
          }

          //             printf("Inverse literals:\n");
          size_t j = 0;
          Lit p;

          for (encoding::wlit_mapt::iterator rit = rootLits.begin();
               rit != rootLits.end(); rit++, j++) {
            if (ith_orl.size() > j + 1) {
              // printf("Lit %lu %lu\n", rit->first,
              p = ith_orl[j + 1].second;
            } else {
              // printf("Lit %lu\n", rit->first);
              p = mkLit(w.solver->nVars(), false);
            }
            newSATVariable(w.solver);
            //~p \/ ~rit->second
            clause.push(~rit->second);
            clause.push(~p);
            w.solver->addClause(clause);
            clause.clear();
            // p \/ rit->second)
            clause.push(rit->second);
            clause.push(p);
            w.solver->addClause(clause);
            clause.clear();
#ifdef __DEBUG__
            printf("\t[%lu] ", rit->first);
            printf("y%d - z%d\n", var(rit->second) + 1, var(p) + 1);
#endif
            // objRootLits[i].push_back(std::pair<uint64_t, Lit>
            // 			     (rit->first, rit->second));
            if (ith_orl.size() <= j + 1) {
              ith_orl.push(
                  std::pair<uint64_t, Lit>(pb._factor * rit->first, p));
            }
          }
        }
      }
    }
  }

#ifdef __DEBUG__
  printRootLit(getFormula()->nObjFunctions());
#endif
  // exit(1);
}

void blockDominatedRegion(size_t solver_id, const YPoint &yp) {
  int nObj = yp.size();
  uint64_t objix[nObj];
}
