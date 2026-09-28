#include "Alg_ParUnsatSatIncHSMO.h"
#include <cmath>
#include <optional>
#include <random>
#include <stdexcept>
#include <thread>

using namespace openwbo;

ParUnsatSatIncHSMO::ParUnsatSatIncHSMO(
    int verb, int weight, int strategy, int enc, int pb, int pbobjf,
    size_t nWorkers, bool clauseSharing, double stride, int expansionOrder)
    : PBtoCNF(verb, weight, strategy, enc, pb, pbobjf),
      UnsatSatMO(verb, weight, strategy, enc, pb, pbobjf),
      PBtoCNFServerMO(verb, weight, strategy, enc, pb, pbobjf),
      UnsatSatIncHSMO(verb, weight, strategy, enc, pb, pbobjf),
      clauseSharing(clauseSharing), stride(stride),
      expansionOrder(expansionOrder) {
  if (nWorkers == 0)
    throw std::invalid_argument("ParUnsatSatIncHSMO requires a worker");
  if (!std::isfinite(stride) || stride < 1.0)
    throw std::invalid_argument("ParUnsatSatIncHSMO requires stride >= 1");
  if (expansionOrder < 0 || expansionOrder > 2)
    throw std::invalid_argument("ParUnsatSatIncHSMO expansion order must be 0, 1, or 2");
  printf("c ParHS optimizer stride: %.6g\n", stride);
  printf("c ParHS optimizer expansion order: %d\n", expansionOrder);
  for (size_t wid = 0; wid < nWorkers; ++wid)
    workers.push_back(std::make_unique<Worker>());
  clausePool =
      std::make_unique<clausesharing::DequeSharedClausesBag>(nWorkers);
}

ParUnsatSatIncHSMO::~ParUnsatSatIncHSMO() {
  delete solver;
  solver = nullptr;
}

bool ParUnsatSatIncHSMO::buildWorkFormula() {
  if (!UnsatSatIncHSMO::buildWorkFormula())
    return false;
  sharedVarCutoff = solver->nVars() - 1;
  for (auto &worker : workers)
    worker->solver.reset(static_cast<Solver *>(solver->clone()));
  return true;
}

void ParUnsatSatIncHSMO::prepareSearch() {
  // Slicing changes between HS rounds, but the encoded variable IDs do not.
  forceSlice(true);
  slicedRoots.clear();
  for (auto &root : objRootLits) {
    auto snapshot = std::make_shared<rootLits::RootLits>();
    for (const auto &entry : *root)
      snapshot->push(entry);
    slicedRoots.push_back(std::move(snapshot));
  }
  forceSlice(false);

  activeBlocks.clear();
  for (auto &entry : solution()) {
    if (entry.second.second < 0) {
      const int before = solver->nVars();
      const YPoint point = entry.second.first.yPoint();
      const int variable = blockStep(point);
      entry.second.second = variable;
      if (variable >= before)
        updates.push_back({variable, blockingClause(point, variable)});
    }
    activeBlocks.insert(entry.second.second);
  }
}

StatusCode ParUnsatSatIncHSMO::searchAgain() {
  setInitialTime(cpuTime());
  prepareSearch();
  searchUnsatSatMO();
  return answerType;
}

bool ParUnsatSatIncHSMO::searchUnsatSatMO() {
  YPoint fence(getFormula()->nObjFunctions());
  if (lowerBound.size()) {
    const YPoint dominator = pareto::dominator(lowerBound);
    if (pareto::dominates(fence, dominator))
      fence = dominator;
  }
  const bool complete = rootedSearch(fence);
  answerType = !complete ? _INTERRUPTED_
                        : solution().size() ? _OPTIMUM_ : _UNSATISFIABLE_;
  return complete;
}

bool ParUnsatSatIncHSMO::rootedSearch(const YPoint &fence) {
  waiting_list::Stack pending;
  pending.insert(fence);
  size_t active = 0;

#pragma omp parallel num_threads(workers.size())
  {
    const size_t wid = omp_get_thread_num();
    while (!getStopSearchFlag()) {
      std::optional<YPoint> next;
      bool finished = false;
#pragma omp critical(parhs_optimizer_work)
      {
        next = pending.try_pop();
        if (next)
          ++active;
        else
          finished = active == 0;
      }
      if (finished)
        break;
      if (!next) {
        std::this_thread::yield();
        continue;
      }
      exploreFence(wid, *next, pending);
#pragma omp critical(parhs_optimizer_work)
      { --active; }
    }
  }

  // Every solver must receive the last guards before the next HS round.
  for (size_t wid = 0; wid < workers.size(); ++wid) {
    synchronize(wid, fence);
    workers[wid]->assumptions.clear();
    nbSatCalls += workers[wid]->satCalls;
    nbSatisfiable += workers[wid]->satisfiable;
    workers[wid]->satCalls = workers[wid]->satisfiable = 0;
  }
  pending.report();
  return !getStopSearchFlag();
}

void ParUnsatSatIncHSMO::synchronize(size_t wid, const YPoint &fence) {
  auto &worker = *workers[wid];
  std::vector<BlockingUpdate> batch;
  std::vector<int> guards;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    batch.assign(updates.begin() + worker.nextUpdate, updates.end());
    guards.assign(activeBlocks.begin(), activeBlocks.end());
  }
  for (const auto &update : batch) {
    while (worker.solver->nVars() <= update.variable)
      worker.solver->newVar();
    vec<Lit> clause;
    for (Lit literal : update.clause)
      clause.push(literal);
    worker.solver->addClause(clause);
    ++worker.nextUpdate;
  }

  worker.assumptions.clear();
  for (Lit literal : blockedVars)
    worker.assumptions.push(~literal);
  for (int variable : guards)
    worker.assumptions.push(mkLit(variable, false));
  std::vector<uint64_t> indices(fence.size());
  openwbo::evalToIndex(fence, indices.data(), slicedRoots);
  for (size_t i = 0; i < indices.size(); ++i)
    if (indices[i] > 0)
      worker.assumptions.push((*slicedRoots[i])[indices[i]].second);
}

std::vector<Lit> ParUnsatSatIncHSMO::blockingClause(const YPoint &point,
                                                   int variable) {
  std::vector<Lit> clause{mkLit(variable, true)};
  for (size_t i = 0; i < point.size(); ++i) {
    size_t index = 0;
    for (const auto &entry : *slicedRoots[i]) {
      if (entry.first > point[i])
        break;
      ++index;
    }
    if (index > 0)
      clause.push_back((*slicedRoots[i])[index].second);
  }
  return clause;
}

void ParUnsatSatIncHSMO::publishModel(size_t wid) {
  const Model model = make_model(workers[wid]->solver->model);
  YPoint point;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!solution().pushSafe(model))
      return;
    point = solution().yPoint();
    const int before = solver->nVars();
    const int variable = blockStep(point);
    solution().note() = variable;
    if (variable >= before)
      updates.push_back({variable, blockingClause(point, variable)});
    activeBlocks.insert(variable);
  }
  std::osyncstream output(std::cout);
  output << "[s" << wid << "] c o " << point << "\n"
      << "[s" << wid << "] c new inner optimal solution (time: "
      << cpuTime() - initialTime << ")\n";
}

void ParUnsatSatIncHSMO::shareWorkerClauses(size_t wid) {
  if (!clauseSharing)
    return;
  auto &worker = *workers[wid];
  auto clauses = worker.solver->getLearntClauses(sharedVarCutoff);
  auto filtered = worker.heuristic.filter(clauses);
  auto received = clausePool->syncSharedClauses(clauses.size(), filtered, wid);
  worker.solver->addLearntClauses(received);
}

void ParUnsatSatIncHSMO::exploreFence(
    size_t wid, const YPoint &fence, waiting_list::WaitingListI &pending) {
  auto &worker = *workers[wid];
  {
    std::osyncstream output(std::cout);
    output << "[s" << wid << "] c new inner harvest. upperLimit: " << fence << "\n";
  }
  while (!getStopSearchFlag()) {
    synchronize(wid, fence);
    shareWorkerClauses(wid);
    if (conflict_limit < 0)
      worker.solver->budgetOff();
    else
      worker.solver->setConfBudget(conflict_limit);
    ++worker.satCalls;
    const lbool sat = worker.solver->solveLimited(worker.assumptions);
    if (sat == l_True) {
      ++worker.satisfiable;
      publishModel(wid);
    } else if (sat == l_Undef) {
      std::osyncstream(std::cout)
          << "[s" << wid << "] c inner budget exhausted. Retrying...\n";
    } else {
      // Capture the core before importing clauses can alter the solver state.
      auto expansions = expansionPoints(wid, fence);
      shareWorkerClauses(wid);
#pragma omp critical(parhs_optimizer_work)
      {
        for (const auto &expanded : expansions)
          pending.insertAndPrune(expanded);
      }
      break;
    }
  }
}

std::vector<YPoint> ParUnsatSatIncHSMO::expansionPoints(
    size_t wid, const YPoint &fence) {
  std::vector<uint64_t> indices(fence.size());
  openwbo::evalToIndex(fence, indices.data(), slicedRoots);
  std::set<int> objectives;
  const auto &core = workers[wid]->solver->conflict;
  for (int i = 0; i < core.size(); ++i) {
    const int objective = getIObjFromLit(core[i]);
    if (objective >= 0)
      objectives.insert(objective);
  }
  std::vector<int> orderedObjectives(objectives.begin(), objectives.end());
  if (expansionOrder == 1) {
    std::reverse(orderedObjectives.begin(), orderedObjectives.end());
  } else if (expansionOrder == 2) {
    static thread_local std::mt19937 rng(std::random_device{}());
    std::shuffle(orderedObjectives.begin(), orderedObjectives.end(), rng);
  }
  std::vector<YPoint> result;
  for (int objective : orderedObjectives) {
    const size_t next = indices[objective] + 1;
    if (next < slicedRoots[objective]->size()) {
      YPoint expanded = fence;
      const uint64_t adjacent = (*slicedRoots[objective])[next].first - 1;
      const uint64_t maximum =
          (*slicedRoots[objective])[slicedRoots[objective]->size() - 1].first - 1;
      const long double scaled =
          std::ceil(static_cast<long double>(fence[objective]) * stride);
      const uint64_t target =
          scaled >= maximum ? maximum : static_cast<uint64_t>(scaled);
      expanded[objective] = std::min(maximum, std::max(adjacent, target));
      result.push_back(std::move(expanded));
    }
  }
  return result;
}

void ParUnsatSatIncHSMO::addDiagnosis(const vec<Lit> &clause) {
  vec<Lit> copy;
  clause.copyTo(copy);
  solver->addClause(copy);
  for (auto &worker : workers) {
    clause.copyTo(copy);
    worker->solver->addClause(copy);
  }
}

void ParUnsatSatIncHSMO::checkSols() {
  std::vector<int> permanent;
  for (const auto &entry : solution())
    if (!marked_sols.count(entry.first))
      permanent.push_back(entry.second.second);
  UnsatSatIncHSMO::checkSols();
  for (auto &worker : workers)
    for (int variable : permanent)
      worker->solver->addClause(mkLit(variable, false));
}
