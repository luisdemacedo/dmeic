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
    std::vector<openwbo::Solution::OneSolution> toAdd = {};
    for (const auto &c : candidates) {
      if (std::any_of(sharedSolutions.begin(), sharedSolutions.end(),
                      [&](const TaggedSolution &tsSol) {
                        openwbo::Solution::OneSolution ySol = tsSol.sol;
                        openwbo::Solution::OneSolution cCopy = c;
                        return ySol.yPoint() == cCopy.yPoint() ||
                               pareto::dominates(ySol.yPoint(), cCopy.yPoint());
                      }))
        continue;

      std::erase_if(sharedSolutions, [&](const TaggedSolution &tsSol) {
        openwbo::Solution::OneSolution ySol = tsSol.sol;
        openwbo::Solution::OneSolution cCopy = c;
        return pareto::dominates(cCopy.yPoint(), ySol.yPoint());
      });

      toAdd.push_back(c);
    }
    for (const auto &tsSol : sharedSolutions)
      if (solverTimestamps[thread_id] < tsSol.ts)
        result.push_back(tsSol.sol);

    for (const auto &c : toAdd)
      sharedSolutions.push_back({c, lastUpdateTime, thread_id});

    solverTimestamps[thread_id] = lastUpdateTime++;

    auto t_copy = clock::now();
    std::vector<TaggedSolution> newSnapshot = sharedSolutions;
    auto t_copy_end = clock::now();

    std::unique_lock<std::mutex> snapshotLock(snapshotMutex);
    snapshotSolutions = std::move(newSnapshot);

    auto t_end = clock::now();
    syncTimeByThread[thread_id] +=
        std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start);
    lockWaitTimeByThread[thread_id] +=
        std::chrono::duration_cast<std::chrono::nanoseconds>(t_lock_acquired -
                                                             t_start);
    snapshotCreationTimeByThread[thread_id] +=
        std::chrono::duration_cast<std::chrono::nanoseconds>(t_copy_end -
                                                             t_copy);
    solutionsPushedByThread[thread_id] += toAdd.size();
    solutionsPulledByThread[thread_id] += result.size();

    DLOG(stderr, "[s%d] Released shared solutions lock...\n",
         omp_get_thread_num());
    DLOG(stderr,
         "[s%d] Time taken for syncSolutions: %lld ms (lock wait: %lld ms, "
         "snapshot creation: %lld "
         "ms)\n",
         omp_get_thread_num(),
         static_cast<long long>(
             std::chrono::duration_cast<std::chrono::milliseconds>(
                 syncTimeByThread[thread_id])
                 .count()),
         static_cast<long long>(
             std::chrono::duration_cast<std::chrono::milliseconds>(
                 lockWaitTimeByThread[thread_id])
                 .count()),
         static_cast<long long>(
             std::chrono::duration_cast<std::chrono::milliseconds>(
                 snapshotCreationTimeByThread[thread_id])
                 .count()));

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
