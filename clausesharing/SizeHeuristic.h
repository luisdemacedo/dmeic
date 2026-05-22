#ifndef SIZE_HEURISTIC_H
#define SIZE_HEURISTIC_H

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

class SizeHeuristic : public IClauseSharingHeuristic {

public:
  SizeHeuristic(size_t size = 8) : cutoff(size) {}

  std::vector<vec<Lit>>
  filter(const std::vector<vec<Lit>> &sharedClauses) override {
    std::vector<vec<Lit>> result;
    for (const auto &clause : sharedClauses) { // Element-wise clone
      if (clause.size() <= cutoff) {
        vec<Lit> copy;
        clause.copyTo(copy);
        result.push_back(std::move(copy));
      }
    }
    return result;
  }

private:
  size_t cutoff;
};

} // namespace clausesharing
#endif
