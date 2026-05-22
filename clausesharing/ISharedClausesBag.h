#ifndef ISHARED_CLAUSES_BAG_H
#define ISHARED_CLAUSES_BAG_H

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include "../DebugLog.h"
#include <atomic>
#include <chrono>
#include <vector>

using NSPACE::Lit;
using NSPACE::vec;

namespace clausesharing {
class ISharedClausesBag {
public:
  virtual std::vector<vec<Lit>>
  syncSharedClauses(size_t unfiltered_count,
                    const std::vector<vec<Lit>> &clauses, int thread_id) = 0;

  std::size_t getSyncs(int thread_id) const { return syncs[thread_id]; }
  std::size_t getNonemptyPushes(int thread_id) const {
    return nonempty_pushes[thread_id];
  }
  std::size_t getNonemptyGrabs(int thread_id) const {
    return nonempty_grabs[thread_id];
  }
  std::size_t getSyncFails(int thread_id) const {
    return sync_fails[thread_id];
  }
  size_t getLargestPush() const { return largest_push; }
  size_t getLargestGrab() const { return largest_grab; }
  size_t getMostClausesInBag() const { return most_clauses_in_bag; }
  size_t getClausesPushed(int thread_id) const {
    return clauses_pushed[thread_id];
  }
  size_t getClausesGrabbed(int thread_id) const {
    return clauses_grabbed[thread_id];
  }
  size_t getClausesFilteredOut(int thread_id) const {
    return clauses_filtered_out[thread_id];
  }
  std::chrono::nanoseconds getSyncTime(int thread_id) const {
    return sync_times[thread_id];
  }
  std::chrono::nanoseconds getLockWaitTime(int thread_id) const {
    return lock_wait_times[thread_id];
  }

protected:
  virtual void push(const std::vector<vec<Lit>> &clauses, int thread_id) = 0;
  virtual std::vector<vec<Lit>> grab(int thread_id) = 0;
  virtual void clean() = 0;
  // metrics
  std::vector<std::size_t> syncs;           // sync (push + grab) was called
  std::vector<std::size_t> nonempty_pushes; // push with something
  std::vector<std::size_t> nonempty_grabs;  // grab returned something
  std::vector<std::size_t> sync_fails;      // sync failed to acquire lock
  std::vector<std::size_t> clauses_pushed;  // number of clauses pushed
  std::vector<std::size_t> clauses_grabbed; // number of clauses grabbed
  std::vector<std::size_t>
      clauses_filtered_out; // number of clauses filtered out by the heuristic
  std::vector<std::chrono::nanoseconds> sync_times; // time spent in sync
  std::vector<std::chrono::nanoseconds>
      lock_wait_times;            // time spent waiting for lock
  size_t largest_push = 0;        // largest push size
  size_t largest_grab = 0;        // largest grab size
  size_t most_clauses_in_bag = 0; // most clauses in bag at once
};

} // namespace clausesharing
#endif
