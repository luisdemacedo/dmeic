#ifndef SHARED_SOLUTIONS_ARCHIVE_H
#define SHARED_SOLUTIONS_ARCHIVE_H

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include "../MOCO.h"
#include "../Pareto.h"
#include "ISharedSolutionsSet.h"
#include <algorithm>
#include <atomic>
#include <deque>
#include <mutex>
#include <vector>

using NSPACE::Lit;
using NSPACE::vec;

namespace solutionsharing {
static constexpr size_t MAX_SOLUTIONS = 100000;

struct TaggedSolution {
  openwbo::Solution::OneSolution sol;
  size_t ts;
  size_t added_by_thread;
  std::atomic<bool> dominated{false};

  TaggedSolution(openwbo::Solution::OneSolution s, size_t t, size_t thread_id)
      : sol(std::move(s)), ts(t), added_by_thread(thread_id), dominated(false) {
  }

  TaggedSolution(const TaggedSolution &) = delete;
  TaggedSolution &operator=(const TaggedSolution &) = delete;
};

class SharedSolutionsArchive : public ISharedSolutionsSet {
  using clock = std::chrono::steady_clock;

public:
  SharedSolutionsArchive(size_t numSolvers) : solverTimestamps(numSolvers, 0) {
    entries.resize(MAX_SOLUTIONS);
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
    size_t sharedBefore = published.load(std::memory_order_acquire);
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
         candidates.size(), sharedBefore,
         published.load(std::memory_order_acquire));
    return result;
  }

  void pushSolutions(std::vector<openwbo::Solution::OneSolution> candidates,
                     size_t thread_id) override {
    auto t_start = clock::now();
    std::vector<TaggedSolution *> toAdd = {};
    size_t n = published.load(std::memory_order_acquire);
    const size_t archiveLimit = archive.size();

    auto inspect = [&](TaggedSolution &entry,
                       const openwbo::YPoint &candidate) {
      if (entry.dominated.load(std::memory_order_acquire))
        return false;

      auto &yp = entry.sol.yPoint();

      if (yp == candidate || pareto::dominates(yp, candidate))
        return true;

      if (pareto::dominates(candidate, yp))
        if (!entry.dominated.exchange(true, std::memory_order_acq_rel))
          liveCount.fetch_sub(1, std::memory_order_release);

      return false;
    };

    for (const auto &c : candidates) {
      bool skip = false;

      openwbo::Solution::OneSolution cCopy = c;

      for (size_t i = 0; i < n && !skip; i++)
        skip = inspect(*entries[i], cCopy.yPoint());

      for (size_t i = n; i < archiveLimit && !skip; i++)
        skip = inspect(archive[i], cCopy.yPoint());

      if (skip)
        continue;

      archive.emplace_back(std::move(cCopy), lastUpdateTime + 1, thread_id);
      toAdd.push_back(&archive.back());
    }

    if (!toAdd.empty()) {
      size_t batchTs = ++lastUpdateTime;
      for (TaggedSolution *taggedSol : toAdd) {
        taggedSol->ts = batchTs;
        if (n < entries.size())
          entries[n++] = taggedSol;
        else
          archiveOverflow.store(true, std::memory_order_release);
      }
      liveCount.fetch_add(toAdd.size(), std::memory_order_release);
      published.store(n, std::memory_order_release);
    }
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
    size_t n = published.load(std::memory_order_acquire);

    auto pullEntry = [&](TaggedSolution &entry) {
      if (!entry.dominated.load(std::memory_order_acquire) &&
          entry.added_by_thread != thread_id &&
          solverTimestamps[thread_id] < entry.ts) {
        result.push_back(entry.sol);
      }
    };

    for (size_t i = 0; i < n; i++)
      pullEntry(*entries[i]);

    for (size_t i = n; i < archive.size(); i++)
      pullEntry(archive[i]);

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
         archive.size(), result.size());

    return result;
  }

  virtual std::vector<std::pair<size_t, openwbo::Solution::OneSolution>>
  getSolutions() override {
    std::vector<std::pair<size_t, openwbo::Solution::OneSolution>> result;
    size_t n = published.load(std::memory_order_acquire);
    bool overflow = archiveOverflow.load(std::memory_order_acquire);

    if (overflow) {
      std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
      if (lock.owns_lock()) {
        DLOG(LogCategory::SolutionSharing, stdout,
             "[shared-solutions] getSolutions: overflow path, acquired lock; "
             "scanning full archive\n");
        for (auto &entry : archive)
          if (!entry.dominated.load(std::memory_order_acquire))
            result.push_back(std::make_pair(entry.added_by_thread, entry.sol));

        DLOG(LogCategory::SolutionSharing, stdout,
             "[shared-solutions] getSolutions: full archive result=%zu\n",
             result.size());
        return result;
      } else {
        DLOG(
            LogCategory::SolutionSharing, stdout,
            "[shared-solutions] getSolutions: overflow path, lock unavailable; "
            "falling back to published prefix, output may be incomplete\n");
        for (size_t i = 0; i < n; i++)
          if (!entries[i]->dominated.load(std::memory_order_acquire))
            result.push_back(
                std::make_pair(entries[i]->added_by_thread, entries[i]->sol));
        return result;
      }
    }

    DLOG(LogCategory::SolutionSharing, stdout,
         "[shared-solutions] getSolutions: normal path; scanning "
         "published prefix\n");
    for (size_t i = 0; i < n; i++)
      if (!entries[i]->dominated.load(std::memory_order_acquire))
        result.push_back(
            std::make_pair(entries[i]->added_by_thread, entries[i]->sol));

    return result;
  }

  virtual std::vector<std::pair<size_t, openwbo::YPoint>>
  getSolutionPoints() override {
    std::vector<std::pair<size_t, openwbo::YPoint>> result;
    size_t n = published.load(std::memory_order_acquire);
    bool overflow = archiveOverflow.load(std::memory_order_acquire);

    if (overflow) {
      std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
      if (lock.owns_lock()) {
        DLOG(LogCategory::SolutionSharing, stdout,
             "[shared-solutions] getSolutions: overflow path, acquired lock; "
             "scanning full archive\n");
        for (auto &entry : archive)
          if (!entry.dominated.load(std::memory_order_acquire))
            result.push_back(
                std::make_pair(entry.added_by_thread, entry.sol.yPoint()));

        DLOG(LogCategory::SolutionSharing, stdout,
             "[shared-solutions] getSolutions: full archive result=%zu\n",
             result.size());
        return result;
      } else {
        DLOG(
            LogCategory::SolutionSharing, stdout,
            "[shared-solutions] getSolutions: overflow path, lock unavailable; "
            "falling back to published prefix, output may be incomplete\n");
        for (size_t i = 0; i < n; i++)
          if (!entries[i]->dominated.load(std::memory_order_acquire))
            result.push_back(std::make_pair(entries[i]->added_by_thread,
                                            entries[i]->sol.yPoint()));
        return result;
      }
    }

    DLOG(LogCategory::SolutionSharing, stdout,
         "[shared-solutions] getSolutions: normal path; scanning "
         "published prefix\n");
    for (size_t i = 0; i < n; i++)
      if (!entries[i]->dominated.load(std::memory_order_acquire))
        result.push_back(std::make_pair(entries[i]->added_by_thread,
                                        entries[i]->sol.yPoint()));

    return result;
  }

  bool empty() const override {
    return liveCount.load(std::memory_order_acquire) == 0;
  }

private:
  std::deque<TaggedSolution> archive;
  std::vector<TaggedSolution *> entries;
  std::atomic<size_t> published{0};
  std::atomic<size_t> liveCount{0};
  std::atomic<bool> archiveOverflow{false};
  size_t lastUpdateTime = 0;
  std::vector<size_t> solverTimestamps;
  mutable std::mutex mutex;
};

} // namespace solutionsharing
#endif
