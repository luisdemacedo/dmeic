#include "Alg_ParPMinimalMO.h"
#include <algorithm> // std::max

using namespace openwbo;
// using namespace NSPACE;
using NSPACE::toLit;

void ParPMinimalMO::search_MO() {
  // Init Structures
  init();
  initWorkers();

  //     printf("\t\tc strategic_search\n");
  //     printf("c eps: %f\n", epsilon);

  // Build solver
  double epsthreshold = 1 + 1e-4;

  buildSolversMO();

  auto resform = std::make_unique<bool[]>(workers.size());
  bool terminate = false;
  nbMCS = 0;

  answerType = _UNKNOWN_;

  bool permanentBlock = false;
  while (!terminate) {
    // encode obj functions

#pragma omp parallel num_threads(workers.size())
    {
      if ((size_t)omp_get_num_threads() != workers.size()) {
        fprintf(stderr, "c ERROR: got %d threads, expected %zu\n",
                omp_get_num_threads(), workers.size());
        exit(_ERROR_);
      }
      resform[omp_get_thread_num()] =
          updateMOFormulationIfSAT(omp_get_thread_num());
    }

    if (std::all_of(resform.get(), resform.get() + workers.size(),
                    std::identity{})) {
      printf("c search\n");
      searchFromRandomizedInitialSolutions();
    } else {
      printf("c No more solutions!\n");
    }
    printf("c Done searching\n");
    printf("c epsilon: %f\n", epsilon);
    printf("c reductionFactor: %f\n", redFactor);
    if ((permanentBlock &&
         std::none_of(resform.get(), resform.get() + workers.size(),
                      std::identity{})) ||
        epsilon <= 1 || redFactor < 0) {
      terminate = true;
      printf("c time to terminate\n");
    } else {
      if (epsilon <= epsthreshold)
        epsilon = 1;
      else
        epsilon = 1 + (epsilon - 1) / redFactor;
      printf("c REENCODE epsilon = %f\n", epsilon);
    }
  }

  if (nondom.size() > 0) {

    if (epsilon <= 1) {
      printf("c LBset = PF\n");
      clearLowerBoundSet();
      for (size_t i = 0; i < nondom.size(); i++)
        updateLowerBoundSet(nondom[i], false);
    }
  } else {
    int nreencodes = 0;
    for (size_t i = 0; i < workers.size(); i++)
      nreencodes += workers[i].nreencodes;
    if (nreencodes == 1)
      clearLowerBoundSet();
  }

  if (sharedSolutions->empty())
    answerType = _UNSATISFIABLE_;
  else
    answerType = _OPTIMUM_;
  printAnswer(answerType);
}

void ParPMinimalMO::searchFromRandomizedInitialSolutions(
    double sampleFraction) {
  assert(sampleFraction > 0.0 && sampleFraction <= 1.0);
  int nObj = maxsat_formula->nObjFunctions();
  const std::uint32_t baseSeed = std::random_device{}();
  std::vector<std::vector<Lit>> objectiveLitPools(nObj);
  std::unordered_map<int, std::vector<std::size_t>> objectivesOfLit;
  std::vector<std::unordered_map<int, std::size_t>> LitToObjectivePosition(
      nObj);

  for (int i = 0; i < nObj; i++) {
    objectiveLitPools[i].resize(getFormula()->getObjFunction(i)->_lits.size());
    for (int j = 0; j < getFormula()->getObjFunction(i)->_lits.size(); j++) {
      objectiveLitPools[i][j] = getFormula()->getObjFunction(i)->_lits[j];
      LitToObjectivePosition[i][getFormula()->getObjFunction(i)->_lits[j].x] =
          j;
      objectivesOfLit[getFormula()->getObjFunction(i)->_lits[j].x].push_back(i);
    }
  }

#pragma omp parallel num_threads(workers.size())
  {
    size_t wid = omp_get_thread_num();
    Worker &w = workers[wid];
    std::seed_seq seed{baseSeed, static_cast<std::uint32_t>(wid)};
    std::mt19937 rng(seed);
    lbool sat = l_False;

    if (!wid) {
      w.assumptions.clear();
      do {
        shareClauses(wid);
        sat = solve(wid);
      } while (sat == l_Undef);
    } else
      do { // Randomizing the starting point
        shareClauses(wid);
        vec<Lit> core;
        for (int i = 0; i < w.solver->conflict.size(); i++)
          core.push(w.solver->conflict[i]);

        if (core.size())
          w.solver->addClause(core);

        std::size_t poolSizeBefore = 0;
        std::size_t poolSizeAfter = 0;

#pragma omp critical(objective_lit_pools)
        {
          for (const auto &pool : objectiveLitPools)
            poolSizeBefore += pool.size();

          for (int i = 0; i < core.size(); i++) {
            Lit lit = core[i];

            for (const size_t &objIdx : objectivesOfLit[lit.x]) {
              const size_t pos = LitToObjectivePosition[objIdx].at(lit.x);
              std::swap(objectiveLitPools[objIdx][pos],
                        objectiveLitPools[objIdx].back());
              LitToObjectivePosition[objIdx][objectiveLitPools[objIdx][pos].x] =
                  pos;
              objectiveLitPools[objIdx].pop_back();
              LitToObjectivePosition[objIdx].erase(lit.x);
            }
            objectivesOfLit.erase(lit.x);
          }

          for (const auto &pool : objectiveLitPools)
            poolSizeAfter += pool.size();
        }

        if (core.size() > 0)
          DLOG(LogCategory::Sampling, stdout,
               "%sc shrinking objective lit pools, core size=%d, before=%zu, "
               "after=%zu, removed=%zu\n",
               getSolverId().c_str(), core.size(), poolSizeBefore,
               poolSizeAfter, poolSizeBefore - poolSizeAfter);

        w.assumptions.clear();
        std::vector<Lit> sampledLits =
            sampleObjectiveLits(objectiveLitPools, sampleFraction, rng);

        for (const auto &lit : sampledLits)
          w.assumptions.push(~lit);

      } while ((sat = solve(wid)) != l_True &&
               (sat == l_Undef || w.solver->conflict.size() > 0));

    if (sat == l_True) {
      w.time1stSol = cpuTime() - initialTime;
      std::osyncstream(std::cout)
          << getSolverId()
          << "c initial solution found (time: " << cpuTime() - initialTime
          << ")\n";
      searchParPMinimalMO(wid);
    } else {
      std::osyncstream(std::cout)
          << getSolverId() << "c no initial solution found\n";
    }
  }
}

void ParPMinimalMO::searchParPMinimalMO(size_t wid) {
  auto &w = workers[wid];
  double runtime = cpuTime();
  int nObj = maxsat_formula->nObjFunctions();

  YPoint ul(nObj);

  lbool sat = l_True;

  w.nbSatCalls1stSol = w.nbSatCalls;

  for (; sat == l_True;) {
    for (; sat == l_True;) {
      Model m = make_model(w.solver->model);
      YPoint yp = evalModel(m);
      w.solutions.push(m);
      ul = yp;
      shareClauses(wid);
      shareSolutions(wid, true);
      blockDominatedRegion(wid, ul);
      std::ostringstream oss;
      oss << ul;
      std::osyncstream(std::cout)
          << getSolverId() << "c o " << oss.str() << "\n";
      runtime = cpuTime();
      printf("%sc new feasible solution (time: %.3f)\n", getSolverId().c_str(),
             runtime - initialTime);
      w.assumptions.clear();
      assumeDominatingRegion(wid, ul);

      shareClauses(wid);
      shareSolutions(wid, true);
      sat = solve(wid);
      while (sat == l_Undef) {
        shareClauses(wid);
        shareSolutions(wid, true);
        sat = solve(wid);
      }
    }
    w.assumptions.clear();
    runtime = cpuTime();
    printf("%sc new optimal solution (time: %.3f)\n", getSolverId().c_str(),
           runtime - initialTime);
    blockDominatedRegion(wid, ul);
    shareClauses(wid);
    shareSolutions(wid, true);
    sat = solve(wid);
    while (sat == l_Undef) {
      shareClauses(wid);
      shareSolutions(wid, true);
      sat = solve(wid);
    }
  }
  return;
}

std::vector<Lit> ParPMinimalMO::sampleObjectiveLits(
    const std::vector<std::vector<Lit>> &remainingObjLits,
    double sampleFraction, std::mt19937 &rng) {
  std::vector<Lit> sampledLits;

#pragma omp critical(objective_lit_pools)
  {
    for (const auto &objLits : remainingObjLits) {
      const size_t sampleSize =
          static_cast<size_t>(std::ceil(objLits.size() * sampleFraction));

      std::sample(objLits.begin(), objLits.end(),
                  std::back_inserter(sampledLits), sampleSize, rng);
    }
  }

  return sampledLits;
}
