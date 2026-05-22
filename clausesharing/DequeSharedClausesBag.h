#ifndef DEQUE_SHARED_CLAUSES_BAG_H
#define DEQUE_SHARED_CLAUSES_BAG_H

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include <algorithm>
#include <atomic>
#include <deque>
#include <mutex>
#include <vector>

#include "ISharedClausesBag.h"

using NSPACE::Lit;
using NSPACE::vec;

namespace clausesharing {
class DequeSharedClausesBag : public ISharedClausesBag {
  using clock = std::chrono::steady_clock;

public:
  DequeSharedClausesBag(size_t n_workers) : last_seen_by_solver(n_workers, 0) {

    syncs.resize(n_workers);
    nonempty_pushes.resize(n_workers);
    nonempty_grabs.resize(n_workers);
    sync_fails.resize(n_workers);
    clauses_pushed.resize(n_workers);
    clauses_grabbed.resize(n_workers);
    clauses_filtered_out.resize(n_workers);
    sync_times.resize(n_workers);
    lock_wait_times.resize(n_workers);
  }

  std::vector<vec<Lit>> syncSharedClauses(size_t unfiltered_count,
                                          const std::vector<vec<Lit>> &clauses,
                                          int thread_id) override {
    auto t_start = clock::now();
    syncs[thread_id]++;
    clauses_filtered_out[thread_id] += unfiltered_count - clauses.size();
    std::unique_lock<std::mutex> lock(mutex); // TODO: Check for contention
    auto t_lock = clock::now();

    std::vector<vec<Lit>> res = grab(thread_id);
    if (!clauses.empty())
      push(clauses, thread_id);
    clean();
    auto t_sync = clock::now();
    sync_times[thread_id] +=
        std::chrono::duration_cast<std::chrono::nanoseconds>(t_sync - t_start);
    lock_wait_times[thread_id] +=
        std::chrono::duration_cast<std::chrono::nanoseconds>(t_lock - t_start);
    return res;
  }

protected:
  std::vector<vec<Lit>> grab(int thread_id) override {
    DLOG(stderr, "[s%d] Grabbing clauses...\n", thread_id);
    clauses_grabbed[thread_id] += deque.size() - last_seen_by_solver[thread_id];

    std::vector<vec<Lit>> result;
    for (size_t i = last_seen_by_solver[thread_id]; i < deque.size(); i++) {
      result.emplace_back();
      deque[i].copyTo(result.back());
    }

    if (!result.empty())
      nonempty_grabs[thread_id]++;
    last_seen_by_solver[thread_id] += result.size();
    DLOG(stderr, "[s%d] Grabbed %zu clauses\n", thread_id, result.size());
    largest_grab = std::max(largest_grab, result.size());
    return result;
  }

  void push(const std::vector<vec<Lit>> &clauses, int thread_id) override {
    DLOG(stderr, "[s%d] Pushing %zu clauses...\n", thread_id, clauses.size());
    nonempty_pushes[thread_id]++;
    clauses_pushed[thread_id] += clauses.size();

    for (const auto &clause : clauses) {
      deque.emplace_back();
      clause.copyTo(deque.back());
    }
    last_seen_by_solver[thread_id] += clauses.size();
    DLOG(stderr, "[s%d] Pushed %zu clauses\n", thread_id, clauses.size());
    largest_push = std::max(largest_push, clauses.size());
    most_clauses_in_bag = std::max(most_clauses_in_bag, deque.size());
    DLOG(stderr, "[s%d] Current deque size is %zu\n", thread_id, deque.size());
  }

protected:
  std::mutex mutex;
  std::deque<vec<Lit>> deque;
  std::vector<size_t> last_seen_by_solver;
  void clean() override {
    DLOG(stderr, "[s%d] Cleaning...\n", omp_get_thread_num());
    std::size_t min_seen = *std::min_element(last_seen_by_solver.begin(),
                                             last_seen_by_solver.end());
    deque.erase(deque.begin(), deque.begin() + min_seen);
    for (auto &seen : last_seen_by_solver)
      seen -= min_seen;
    DLOG(stderr, "[s%d] Cleaned up to index %zu, new size is %zu\n",
         omp_get_thread_num(), min_seen, deque.size());
    DLOG(stderr, "[s%d] Updated last_seen_by_solver: ", omp_get_thread_num());
    for (size_t i = 0; i < last_seen_by_solver.size(); i++) {
      DLOG(stderr, "Thread %zu: %zu ", i, last_seen_by_solver[i]);
    }
    DLOG(stderr, "\n");
  }
};

} // namespace clausesharing
#endif
