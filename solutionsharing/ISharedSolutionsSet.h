#ifndef ISHARED_SOLUTIONS_SET_H
#define ISHARED_SOLUTIONS_SET_H

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include "../DebugLog.h"
#include "../MOCO.h"
#include <atomic>
#include <chrono>
#include <vector>

using NSPACE::Lit;
using NSPACE::vec;

namespace solutionsharing {
class ISharedSolutionsSet {
public:
  virtual std::vector<openwbo::Solution::OneSolution>
  syncSolutions(const std::vector<openwbo::Solution::OneSolution> candidates,
                size_t thread_id, bool alsoPull) = 0;

  virtual void
  pushSolutions(const std::vector<openwbo::Solution::OneSolution> candidates,
                size_t thread_id) = 0;

  virtual std::vector<openwbo::Solution::OneSolution>
  pullSolutions(size_t thread_id) = 0;

  virtual std::vector<std::pair<size_t, openwbo::Solution::OneSolution>>
  getSolutions() = 0;

  virtual std::vector<std::pair<size_t, openwbo::YPoint>>
  getSolutionPoints() = 0;

  virtual bool empty() const = 0;

  std::size_t getSolutionsPushed(int thread_id) const {
    return solutionsPushedByThread[thread_id];
  }

  std::size_t getSolutionsPulled(int thread_id) const {
    return solutionsPulledByThread[thread_id];
  }

  std::chrono::nanoseconds getSyncTime(int thread_id) const {
    return syncTimeByThread[thread_id];
  }

  std::chrono::nanoseconds getLockWaitTime(int thread_id) const {
    return lockWaitTimeByThread[thread_id];
  }

  std::chrono::nanoseconds getSnapshotCreationTime(int thread_id) const {
    return snapshotCreationTimeByThread[thread_id];
  }

  std::chrono::nanoseconds getPushTime(int thread_id) const {
    return pushTimeByThread[thread_id];
  }

  std::chrono::nanoseconds getPullTime(int thread_id) const {
    return pullTimeByThread[thread_id];
  }

  size_t getNumWorkers() const { return solutionsPushedByThread.size(); }

protected:
  std::vector<std::size_t> solutionsPushedByThread;
  std::vector<std::size_t> solutionsPulledByThread;
  std::vector<std::chrono::nanoseconds> syncTimeByThread;
  std::vector<std::chrono::nanoseconds> pushTimeByThread;
  std::vector<std::chrono::nanoseconds> pullTimeByThread;
  std::vector<std::chrono::nanoseconds> lockWaitTimeByThread;
  std::vector<std::chrono::nanoseconds> snapshotCreationTimeByThread;
};

} // namespace solutionsharing
#endif
