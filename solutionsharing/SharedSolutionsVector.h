#ifndef ISHARED_SOLUTIONS_VECTOR_H
#define ISHARED_SOLUTIONS_VECTOR_H

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

class SharedSolutionVector : public ISharedSolutionsSet {
  using clock = std::chrono::steady_clock;

public:
  SharedSolutionVector(size_t numSolvers) : solverTimestamps(numSolvers, 0) {
    solutionsPushedByThread.resize(numSolvers);
    solutionsPulledByThread.resize(numSolvers);
    syncTimeByThread.resize(numSolvers);
    lockWaitTimeByThread.resize(numSolvers);
    snapshotCreationTimeByThread.resize(numSolvers);
  }

  virtual std::vector<openwbo::Solution::OneSolution>
  syncSolutions(const std::vector<openwbo::Solution::OneSolution> candidates,
                size_t thread_id, bool blocking) override {
    auto t_start = clock::now();
    std::unique_lock<std::mutex> lock(mutex, std::defer_lock);
    DLOG(stderr, "[s%d] Trying to acquire shared solutions lock...\n",
         omp_get_thread_num());
    if (blocking)
      lock.lock();
    else if (!lock.try_lock())
      return {}; // Return empty if we can't acquire the lock
                 //
    auto t_lock_acquired = clock::now();
    DLOG(stderr, "[s%d] Acquired shared solutions lock...\n",
         omp_get_thread_num());

    std::vector<openwbo::Solution::OneSolution> result = {};
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

      toAdd.push_back({cCopy, lastUpdateTime, thread_id});
    }

    for (const auto &tsSol : sharedSolutions)
      if (solverTimestamps[thread_id] < tsSol.ts)
        result.push_back(tsSol.sol);

    size_t i = 0;
    std::erase_if(sharedSolutions, [&](const auto &) { return toRemove[i++]; });
    sharedSolutions.insert(sharedSolutions.end(), toAdd.begin(), toAdd.end());
    solverTimestamps[thread_id] = lastUpdateTime++;

    auto t_copy = clock::now();
    std::vector<TaggedSolution> newSnapshot = sharedSolutions;
    auto t_copy_end = clock::now();

    std::unique_lock<std::mutex> snapshotLock(snapshotMutex);
    snapshotSolutions = std::move(newSnapshot);

    auto t_end = clock::now();

    auto callSyncTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start);
    auto callLockWaitTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(t_lock_acquired -
                                                             t_start);
    auto callSnapshotCreationTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(t_copy_end -
                                                             t_copy);
    auto callHeldTime = std::chrono::duration_cast<std::chrono::nanoseconds>(
        t_end - t_lock_acquired);

    syncTimeByThread[thread_id] += callSyncTime;
    lockWaitTimeByThread[thread_id] += callLockWaitTime;
    snapshotCreationTimeByThread[thread_id] += callSnapshotCreationTime;
    solutionsPushedByThread[thread_id] += toAdd.size();
    solutionsPulledByThread[thread_id] += result.size();

    DLOG(stderr,
         "[s%d] syncSolutions call: %lld ms, wait: %lld ms, held: %lld ms, "
         "snapshot: %lld ms, "
         "candidates: %zu, already shared (before): %zu, already shared "
         "(after): %zu, pulled: %zu, pushed: "
         "%zu\n",
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
         static_cast<long long>(
             std::chrono::duration_cast<std::chrono::milliseconds>(
                 callSnapshotCreationTime)
                 .count()),
         candidates.size(),
         sharedSolutions.size() +
             std::count(toRemove.begin(), toRemove.end(), true),
         sharedSolutions.size(), result.size(), toAdd.size());
    return result;
  }

  virtual std::vector<std::pair<size_t, openwbo::Solution::OneSolution>>
  getSolutions() override {
    std::vector<std::pair<size_t, openwbo::Solution::OneSolution>> result;
    std::unique_lock<std::mutex> lock(mutex, std::defer_lock);
    if (!lock.try_lock()) {
      DLOG(stderr,
           "[s%d] Could not acquire lock to get solutions, returning "
           "snapshot.\n",
           omp_get_thread_num());
      std::unique_lock<std::mutex> snapshotLock(snapshotMutex);
      for (const auto &tsSol : snapshotSolutions)
        result.push_back(std::make_pair(tsSol.added_by_thread, tsSol.sol));
    } else {
      for (const auto &tsSol : sharedSolutions)
        result.push_back(std::make_pair(tsSol.added_by_thread, tsSol.sol));
    }
    return result;
  }

private:
  std::vector<TaggedSolution> sharedSolutions;
  std::vector<TaggedSolution> snapshotSolutions;
  std::mutex mutex;
  std::mutex snapshotMutex;
  size_t lastUpdateTime = 0;
  std::vector<size_t> solverTimestamps;
};

} // namespace solutionsharing
#endif
