#ifndef ALL_CLAUSES_HEURISTIC_H
#define ALL_CLAUSES_HEURISTIC_H

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include "IClauseSharingHeuristic.h"
#include <vector>

using NSPACE::Lit;
using NSPACE::vec;

namespace clausesharing {

class AllClausesHeuristic : public IClauseSharingHeuristic {
public:
  std::vector<vec<Lit>>
  filter(const std::vector<vec<Lit>> &sharedClauses) override {
    std::vector<vec<Lit>> result;
    for (const auto &clause : sharedClauses) {
      vec<Lit> copy;
      clause.copyTo(copy);
      result.push_back(std::move(copy));
    }
    return result;
  }
};

} // namespace clausesharing
#endif
