#ifndef SHARED_SOLUTIONS_VECTOR_H
#define SHARED_SOLUTIONS_VECTOR_H

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include "../MOCO.h"
#include "../Pareto.h"
#include "ISharedSolutionsSet.h"
#include <algorithm>
#include <mutex>
#include <vector>

using NSPACE::Lit;
using NSPACE::vec;

namespace solutionsharing {
struct TaggedSolution {
  openwbo::Solution::OneSolution sol;
  size_t ts;
  size_t added_by_thread;
};

class SharedSolutionsVector : public ISharedSolutionsSet {
  using clock = std::chrono::steady_clock;

public:
  SharedSolutionsVector(size_t numSolvers) : solverTimestamps(numSolvers, 0) {
    solutionsPushedByThread.resize(numSolvers);
    solutionsPulledByThread.resize(numSolvers);
    syncTimeByThread.resize(numSolvers);
    lockWaitTimeByThread.resize(numSolvers);
    pullTimeByThread.resize(numSolvers);
    pushTimeByThread.resize(numSolvers);
  }

  virtual std::vector<openwbo::Solution::OneSolution>
  syncSolutions(const std::vector<openwbo::Solution::OneSolution> candidates,
                size_t thread_id, bool alsoPull) override {
    auto t_start = clock::now();
    size_t sharedBefore = sharedSolutions.size();
    std::unique_lock<std::mutex> lock(mutex, std::defer_lock);
    DLOG(LogCategory::SolutionSharing, stdout,
         "[s%d] Trying to acquire shared solutions lock...\n",
         omp_get_thread_num());
    lock.lock();

    auto t_lock_acquired = clock::now();
    DLOG(LogCategory::SolutionSharing, stdout,
         "[s%d] Acquired shared solutions lock...\n", omp_get_thread_num());

    std::vector<openwbo::Solution::OneSolution> result = {};

    if (alsoPull)
      result = pullSolutions(thread_id);
    pushSolutions(candidates, thread_id);

    auto t_end = clock::now();

    auto callSyncTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start);
    auto callLockWaitTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(t_lock_acquired -
                                                             t_start);
    auto callHeldTime = std::chrono::duration_cast<std::chrono::nanoseconds>(
        t_end - t_lock_acquired);

    syncTimeByThread[thread_id] += callSyncTime;
    lockWaitTimeByThread[thread_id] += callLockWaitTime;

    DLOG(LogCategory::SolutionSharing, stdout,
         "[s%d] syncSolutions call: %lld ms, wait: %lld ms, held: %lld ms, "
         "candidates: %zu, shared solutions (before): %zu, shared solutions "
         "(after): %zu\n",
         omp_get_thread_num(),
         static_cast<long long>(
             std::chrono::duration_cast<std::chrono::milliseconds>(callSyncTime)
                 .count()),
         static_cast<long long>(
             std::chrono::duration_cast<std::chrono::milliseconds>(
                 callLockWaitTime)
                 .count()),
         static_cast<long long>(
             std::chrono::duration_cast<std::chrono::milliseconds>(callHeldTime)
                 .count()),
         candidates.size(), sharedBefore, sharedSolutions.size());
    return result;
  }

  void pushSolutions(std::vector<openwbo::Solution::OneSolution> candidates,
                     size_t thread_id) override {
    auto t_start = clock::now();
    std::vector<TaggedSolution> toAdd = {};
    std::vector<bool> toRemove(sharedSolutions.size(), false);

    for (const auto &c : candidates) {
      openwbo::Solution::OneSolution cCopy = c;
      const auto &cYPoint = cCopy.yPoint();
      bool skip = false;
      for (size_t i = 0; i < sharedSolutions.size(); ++i) {
        auto &taggedSol = sharedSolutions[i];
        auto &tsSol = taggedSol.sol;
        auto &sYPoint = tsSol.yPoint();

        if (sYPoint == cYPoint || pareto::dominates(sYPoint, cYPoint)) {
          skip = true;
          break;
        } else if (pareto::dominates(cYPoint, sYPoint)) {
          toRemove[i] = true;
        }
      }
      if (skip)
        continue;

      toAdd.push_back({cCopy, 0, thread_id});
    }

    size_t i = 0;
    std::erase_if(sharedSolutions, [&](const auto &) { return toRemove[i++]; });
    if (!toAdd.empty()) {
      size_t batchTs = ++lastUpdateTime;
      for (auto &taggedSol : toAdd)
        taggedSol.ts = batchTs;
    }
    sharedSolutions.insert(sharedSolutions.end(), toAdd.begin(), toAdd.end());
    solverTimestamps[thread_id] = lastUpdateTime;

    auto t_end = clock::now();

    solutionsPushedByThread[thread_id] += toAdd.size();
    auto callPushTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start);
    pushTimeByThread[thread_id] += callPushTime;
    DLOG(LogCategory::SolutionSharing, stdout,
         "[s%d] pushSolutions call: %lld ms, candidates: %zu, pushed: %zu\n",
         omp_get_thread_num(),
         static_cast<long long>(
             std::chrono::duration_cast<std::chrono::milliseconds>(callPushTime)
                 .count()),
         candidates.size(), toAdd.size());
  }

  std::vector<openwbo::Solution::OneSolution>
  pullSolutions(size_t thread_id) override {
    auto t_start = clock::now();
    std::vector<openwbo::Solution::OneSolution> result;
    for (const auto &tsSol : sharedSolutions)
      if (tsSol.added_by_thread != thread_id &&
          solverTimestamps[thread_id] < tsSol.ts)
        result.push_back(tsSol.sol);

    auto t_end = clock::now();
    solutionsPulledByThread[thread_id] += result.size();
    auto callPullTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start);

    pullTimeByThread[thread_id] += callPullTime;

    DLOG(LogCategory::SolutionSharing, stdout,
         "[s%d] pullSolutions call: %lld ms, solutions in pool: %zu, pulled: "
         "%zu\n",
         omp_get_thread_num(),
         static_cast<long long>(
             std::chrono::duration_cast<std::chrono::milliseconds>(callPullTime)
                 .count()),
         sharedSolutions.size(), result.size());

    return result;
  }
  virtual std::vector<std::pair<size_t, openwbo::Solution::OneSolution>>
  getSolutions() override {
    std::vector<std::pair<size_t, openwbo::Solution::OneSolution>> result;
    // Ignoring lock acquisition
    for (const auto &tsSol : sharedSolutions)
      result.push_back(std::make_pair(tsSol.added_by_thread, tsSol.sol));
    return result;
  }

  bool empty() const override {
    std::scoped_lock lock(mutex);
    return sharedSolutions.empty();
  }

private:
  std::vector<TaggedSolution> sharedSolutions;
  mutable std::mutex mutex;
  size_t lastUpdateTime = 0;
  std::vector<size_t> solverTimestamps;
};

} // namespace solutionsharing
#endif
