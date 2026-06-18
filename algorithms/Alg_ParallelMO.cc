#include "Alg_ParallelMO.h"

using namespace openwbo;
using NSPACE::toLit;

StatusCode ParallelMO::search() {
  search_MO();
  printf("done\n");
  for (auto &w : workers)
    w.solver->budgetOff();
  return answerType;
}

void ParallelMO::updateStats() {
  int nev = 0, nec = 0, nerv = 0;

  for (int i = 0; i < getFormula()->nObjFunctions(); i++) {
    // Printing from the first solver in the portfolio, since all solvers should
    // have the same encoding sizes
    if (enc_is_kp_based(0))
      workers[0].kps[i].getEncodeSizes(&nev, &nec, &nerv);
    else if (workers[0].encoder.getPBEncoding() == _PB_GTE_)
      workers[0].gtes[i].getEncodeSizes(&nev, &nec, &nerv);
    workers[0].nbEncVars += nev;
    workers[0].nbEncClauses += nec;
    workers[0].nbEncRootVars += nerv;
    fprintf(stdout, "c objstats %3d %5d %8d %9d\n", i + 1, nev, nec, nerv);
  }
}

// TODO: add stats
void ParallelMO::printStats() {
  double totalTime = cpuTime();

  FILE *f = stdout;

  std::cout << "c ------- " << std::endl;
  fprintf(f, "c lobjstats obj nvars nclauses nrootvars\n");
  updateStats();

  std::cout << "c ------- " << std::endl;
  fprintf(f,
          "clrunstats     %18s %12s %12s %12s %12s %12s %18s %18s %18s %20s "
          "%18s %8s %8s\n",
          "nsatcalls_1stSol", "nsatcalls", "ncalls", "n_eff_sols", "n_nondom",
          "n_prob_vars", "n_prob_clauses", "n_enc_vars(sum)",
          "n_enc_clauses(sum)", "n_enc_rootvars(sum)", "n_reencodes", "rapprox",
          "nobj");
  for (size_t idx = 0; idx < workers.size(); idx++)
    fprintf(f,
            "crunstats %4s %18d %12d %12d %12d %12d %12d %18d %18d %18d %20d "
            "%18d %8.4f %8d\n",
            std::format("[s{}]", idx).c_str(), workers[idx].nbSatCalls1stSol,
            workers[idx].nbSatisfiable, workers[idx].nbSatCalls,
            workers[idx].solutions.size(), workers[idx].solutions.size(),
            getFormula()->nVars(), getFormula()->nHard(),
            workers[idx].nbEncVars, workers[idx].nbEncClauses,
            workers[idx].nbEncRootVars, workers[idx].nbReencodes, repsilon,
            getFormula()->nObjFunctions());

  // if (getShareClauses()) {
  std::cout << "c ------- " << std::endl;
  fprintf(f,
          "cclausesharingstats       %20s %20s %20s %20s %20s %22s %24s %24s\n",
          "nsynccalls", "nnonempty_pushes", "nnonempty_grabs",
          "nclauses_pushed", "nclauses_grabbed", "nclauses_filtered_out",
          "time_spent_syncing (ms)", "time_waiting_lock (ms)");
  for (size_t idx = 0; idx < sharedLearntClauses->getNumWorkers(); idx++) {
    auto syncTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        sharedLearntClauses->getSyncTime(idx));
    auto lockWaitTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        sharedLearntClauses->getLockWaitTime(idx));

    fprintf(f,
            "%-20s %4s %20zu %20zu %20zu %20zu %20zu %22zu %24lld "
            "%24lld\n",
            "clausesharingstats", std::format("[s{}]", idx).c_str(),
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
  fprintf(f, "clausesharingstats %16zu %16zu %20zu\n",
          sharedLearntClauses->getLargestPush(),
          sharedLearntClauses->getLargestGrab(),
          sharedLearntClauses->getMostClausesInBag());
  // }

  std::cout << "c ------- " << std::endl;
  fprintf(f, "ccsolutionsharingstats      %20s %20s %24s %24s %20s %20s\n",
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
  for (size_t idx = 0; idx < workers.size(); idx++)
    fprintf(f, "ctimestats %4s %12.2f %12.2f\n",
            std::format("[s{}]", idx).c_str(), workers[idx].time1stSol,
            totalTime - workers[idx].initialTime);
}

void ParallelMO::printAnswer(int type) {
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
    // printApproxRatio();
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

void ParallelMO::printSolutions() {
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
void ParallelMO::buildSolversMO() {
  DLOG(stdout,
       "c [ParallelMO] buildSolversMO -- nVars: "
       "%d nHard: %d nSoft: %d nPB: %d nObj: %d\n",
       getFormula()->nVars(), getFormula()->nHard(), getFormula()->nSoft(),
       getFormula()->nPB(), getFormula()->nObjFunctions());
#pragma omp parallel for
  for (size_t idx = 0; idx < workers.size(); idx++) {
    auto &w = workers[idx];
    std::osyncstream(std::cout) << getSolverId() << " c Building solver\n";
    vec<bool> seen;
    seen.growTo(getFormula()->nVars(), false);

    w.solver = newSATSolver();

    for (int i = 0; i < getFormula()->nVars(); i++)
      newSATVariable(w.solver);

    for (int i = 0; i < getFormula()->nHard(); i++)
      w.solver->addClause(getFormula()->getHardClause(i).clause);

    //  printf("c Encode PB constraints\n");

    for (int i = 0; i < getFormula()->nPB(); i++) {
      encoding::Encoder *enc = new encoding::Encoder(
          _INCREMENTAL_NONE_, encoding, _AMO_LADDER_, pb_encoding);

      assert(getFormula()->getPBConstraint(i)->_sign);

      enc->encodePB(w.solver, getFormula()->getPBConstraint(i)->_lits,
                    getFormula()->getPBConstraint(i)->_coeffs,
                    getFormula()->getPBConstraint(i)->_rhs);
      delete enc;
    }

    //  printf("c Encode cardinality constraints\n");
    for (int i = 0; i < getFormula()->nCard(); i++) {
      encoding::Encoder *enc = new encoding::Encoder(
          _INCREMENTAL_NONE_, encoding, _AMO_LADDER_, pb_encoding);

      if (getFormula()->getCardinalityConstraint(i)->_rhs == 1) {
        enc->encodeAMO(w.solver,
                       getFormula()->getCardinalityConstraint(i)->_lits);
      } else {

#ifdef __DEBUG__
        printf(
            "c [ParallelMO] buildSolversMO encodeCardinality() constraint\n\t");
        getFormula()->getCardinalityConstraint(i)->my_print(
            getFormula()->getIndexToName());
#endif

        enc->encodeCardinality(w.solver,
                               getFormula()->getCardinalityConstraint(i)->_lits,
                               getFormula()->getCardinalityConstraint(i)->_rhs);
      }

      delete enc;
    }

    vec<Lit> clause;
    // printf("c Encode soft constraints\n");
    for (int i = 0; i < getFormula()->nSoft(); i++) {
      clause.clear();
      getFormula()->getSoftClause(i).clause.copyTo(clause);

      for (int j = 0; j < getFormula()->getSoftClause(i).relaxation_vars.size();
           j++) {
        clause.push(getFormula()->getSoftClause(i).relaxation_vars[j]);
      }

      w.solver->addClause(clause);
    }

    // printf("c Encode objective functions\n");

    int nObj = getFormula()->nObjFunctions();

    for (int di = 0; di < nObj; di++)
      w.fubs[di] = 0;
    std::set<Lit> fixedVars;
    // thread-local sync_first
    for (int i = 0; i < getFormula()->nInitialVars(); i++)
      if (w.solver->value(i) != l_Undef)
        fixedVars.insert(mkLit(i, true));
    w.encoder.kpa_fixed_vars(fixedVars);
    w._nb_encoded_vars_initial = w.solver->nVars();
  }
}

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
    return w.solver->solveLimited(w.assumptions);
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

bool ParallelMO::firstSolution(size_t wid) {
  Worker &w = workers[wid];
  if (w.first.model().size())
    return true;

  double before1stsol = cpuTime();
  std::osyncstream(std::cout) << getSolverId() << " c first call to solver\n";

#pragma omp atomic
  nbSatCalls++;

  w.solver->budgetOff();
  lbool res = w.solver->solveLimited(w.assumptions);
  double total_time = cpuTime() - before1stsol;
  std::osyncstream(std::cout)
      << getSolverId() << " c first call to solver time:" << total_time << "\n";
  if (res != l_True)
    return false;

#pragma omp atomic
  nbSatisfiable++;

  w.first = Solution::OneSolution{&w.solutions, make_model(w.solver->model)};
  return true;
}

bool ParallelMO::updateMOFormulationIfSAT(size_t wid) {
  printf("c [updateMOFormulationIfSAT]\n");
  //     solver->my_print();

  if (!firstSolution(wid))
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
    std::osyncstream(std::cout) << getSolverId() << " c updateMOEncoding\n";
    //         solver->my_print();
    //         assumptions.clear();
    for (int i = 0; i < getFormula()->nObjFunctions(); i++) {
      PBObjFunction pb{*getFormula()->getObjFunction(i)};
      auto factor = pb._factor;

      auto &ith_orl = *w.objRootLits[i].get();

      if (enc_is_kp_based(wid) || w.encoder.getPBEncoding() == _PB_GTE_) {

        if (w.objRootLits[i] && w.objRootLits[i]->size() > 0) {
          //                     objRootLits[i].clear();

          std::osyncstream(std::cout)
              << getSolverId() << " c clear encoding of obj. funct. " << i
              << "\n";
          if (enc_is_kp_based(wid)) {
            //                         kps[i].clearedEncoding(solver);
            // TODO
          }
        }

        std::osyncstream(std::cout)
            << getSolverId() << " c encode (function " << i
            << " upper bound: " << w.fubs[i] << ")\n";
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
#pragma omp barrier
#pragma omp single
        getFormula()->replaceObjFunction(
            i, make_unique<PBObjFunction>(std::move(pb)));
        int o = 0;
        vec<Lit> clause;
        // order encoding
        if (enc_is_kp_based(wid)) {
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

void ParallelMO::shareSolutions(size_t wid, bool alsoPull) {
  Worker &w = workers[wid];
  std::vector<openwbo::Solution::OneSolution> localFront = {};

  for (auto &[_, s] : w.solutions)
    localFront.push_back(s.first);

  std::vector<openwbo::Solution::OneSolution> receivedFront =
      sharedSolutions->syncSolutions(localFront, wid, alsoPull);

  for (auto &sol : receivedFront) {
    blockDominatedRegion(wid, sol.yPoint());
    w.solutions.pushSafe(sol.model());
  }
}

void ParallelMO::shareClauses(size_t wid) {
  if (!getShareClauses())
    return;

  Worker &w = workers[wid];
  std::vector<vec<Lit>> clauses = w.solver->getLearntClauses(-1); // 0-indexed
  std::vector<vec<Lit>> filteredClauses = w.sharingHeuristic->filter(clauses);
  std::vector<vec<Lit>> receivedClauses =
      sharedLearntClauses->syncSharedClauses(clauses.size(), filteredClauses,
                                             wid);
  w.solver->addLearntClauses(receivedClauses);
}

/*assumes the region that dominates the point given by objix*/
void ParallelMO::assumeDominatingRegion(size_t wid, uint64_t *objix, int nObj) {
  Worker &w = workers[wid];
  for (int di = 0; di < nObj; di++) {
    int j = objix[di];
    if (j > 0) {
      w.assumptions.push((*w.objRootLits[di])[j].second);
#ifdef __DEBUG__
      printf("assume all %d [%lu][var: %d]\n", di,
             (*w.objRootLits[di])[j].first,
             var((*w.objRootLits[di])[j].second));
#endif
    }
  }
}

/*assumes the region that dominates the point given by yp*/
int ParallelMO::assumeDominatingRegion(size_t wid, const YPoint &yp) {
  Worker &w = workers[wid];
  auto nObj = yp.size();
  YPoint yp1 = yp;
  uint64_t objix[nObj];
  int pushed = 0;
  evalToIndex(wid, yp1, objix);
  for (YPoint::size_type di = 0; di < nObj; di++) {
    int j = objix[di];
    if (j > 0 && j < (int)(*w.objRootLits[di]).size()) {
      w.assumptions.push((*w.objRootLits[di]).at(j).second);
      pushed++;
#ifdef __DEBUG__
      printf("assume all %d [%lu][var: %d]\n", di,
             (*w.objRootLits[di])[j].first,
             var((*w.objRootLits[di])[j].second));
#endif
    }
  }
  return pushed;
}

void ParallelMO::blockDominatedRegion(size_t wid, uint64_t *objix, int nObj) {
  // at least one of the functions will be strictly lower than the
  // values correspoding to objix. objix should be filled by
  // evalToIndex.

  Worker &w = workers[wid];
  vec<Lit> d_cl;
  for (int di = 0; di < nObj; di++) {
    int j = objix[di];
    // the j = 0 entry is never used. Accomplished by the sentinel
    // placed at the beggining of objRootLits.
    if (j > 0) {
      d_cl.push((*w.objRootLits[di])[j].second);
      //                 printf("block z%d\n",
      //                 var(w.objRootLits[di][j].second)+1);
#ifdef __DEBUG__
      printf("c block %d [%lu][var: %d]\n", di, w.objRootLits[di][j].first,
             var(w.objRootLits[di][j].second));
#endif
    }
  }
  w.solver->addClause(d_cl);
}

void ParallelMO::blockDominatedRegion(size_t solver_id, const YPoint &yp) {
  int nObj = yp.size();
  uint64_t objix[nObj];
  Worker &w = workers[solver_id];

  // computing the indexes given the objective value. This should be
  //  abstracted away...  For each objective, find the index of the
  //  largest key below or equal to each entry in yp, or 0 if no such
  //  entry exists.

  for (int iObj = 0; iObj < nObj; iObj++) {
    objix[iObj] = 0;
    if (w.objRootLits[iObj])
      for (auto const &el : *w.objRootLits[iObj])
        if (yp[iObj] >= el.first)
          objix[iObj]++;
        else
          break;
  }
  blockDominatedRegion(solver_id, objix, yp.size());
}

void ParallelMO::evalToIndex(size_t wid, const YPoint &yp, uint64_t *objix) {
  Worker &w = workers[wid];
  openwbo::evalToIndex(yp, objix, w.objRootLits);
}

void ParallelMO::evalToIndex(size_t wid, uint64_t *objv, uint64_t *objix) {
  Worker &w = workers[wid];
  for (int di = 0; di < getFormula()->nObjFunctions(); di++) {
    objix[di] = 0;
    size_t i = 1;
    if (w.objRootLits[di] != NULL)
      while (i <= (*w.objRootLits[di]).size() &&
             objv[di] >= (*w.objRootLits[di])[i].first) {
        objix[di] = i;
        i++;
      }
  }
}

void ParallelMO::printApproxRatio() {
  int d = getFormula()->nObjFunctions();
  auto sols = sharedSolutions->getSolutions();
  auto yPoints = sols | std::views::values |
                 std::views::transform(&Solution::OneSolution::yPoint);
  double realeps = 0;
  double pteps, ptepsi;
  for (size_t j = 0; j < LBset.size(); j++) {
    pteps = DBL_MAX;
    for (size_t i = 0; i < yPoints.size(); i++) {
      ptepsi = 1;
      for (int di = 0; di < d; di++) {
        // se nao for 0 (se for, entao o valor acima e' o minimo da funcao
        // objectivo di) e se o racio for maior do que dos outros di e se
        // LB*epsilon > LB+1 (para contemplar os casos em que epsilon_approx <
        // 2)
        if (LBset[j][di] > 0 && float(yPoints[i][di]) / LBset[j][di] > ptepsi &&
            LBset[j][di] * (float(yPoints[i][di]) / LBset[j][di]) >=
                LBset[j][di] + 1) {
          ptepsi = float(yPoints[i][di]) / LBset[j][di];
        }
      }
      //             printf("%f < %f ?\n", ptepsi, pteps);
      if (ptepsi < pteps)
        pteps = ptepsi;
    }
    if (pteps > realeps)
      realeps = pteps;
  }

  std::osyncstream(std::cout) << "c ------- " << "\n";
  std::osyncstream(std::cout) << "c observed and expected ratio" << "\n";
  std::osyncstream(std::cout) << std::format("c tapprox <= {:.4f}\n", realeps);
  std::osyncstream(std::cout)
      << std::format("c eapprox <= {:.4f}\n", expepsilon);
  std::osyncstream(std::cout) << "c ------- " << "\n";
  repsilon = realeps;
}
