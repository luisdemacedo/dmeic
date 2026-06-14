#ifndef ICLAUSE_SHARING_HEURISTIC_H
#define ICLAUSE_SHARING_HEURISTIC_H

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include <vector>

using NSPACE::Lit;
using NSPACE::vec;

namespace clausesharing {
class IClauseSharingHeuristic {
public:
  virtual ~IClauseSharingHeuristic() = default;
  virtual std::vector<vec<Lit>>
  filter(const std::vector<vec<Lit>> &sharedClauses) = 0;
};

} // namespace clausesharing
#endif
