#ifndef PARUNSATSATMO
#define PARUNSATSATMO
// #define PARTIAL
#include "core/SolverTypes.h"
#include <memory>
#ifndef PARTIAL

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include "../Encoder.h"
#include "../MaxSAT.h"
#include "../Pareto.h"
#include "../partition.h"
#include "../waiting_list/waiting_list.h"
#include "./Alg_ParallelMO.h"
#include "./Alg_ServerMO.h"
#include "utils/System.h"
#include <algorithm> // std::max
#include <random>
#include <utility>

#define MAXDIM 50

namespace openwbo {
using namespace partition;

class ParUnsatSatMO : public virtual ParallelMO {

public:
  ParUnsatSatMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_,
                int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_,
                int pb = _PB_SWC_, int pbobjf = _PB_GTE_, int core_budget = -1,
                size_t nWorkers = 2, bool clausesharing = false)
      : ParallelMO(verb, weight, strategy, enc, pb, pbobjf) {}

  std::vector<MyPartition> generate();
  MyPartition mix(std::vector<MyPartition>);

  bool buildWorkFormula() override;
  StatusCode searchAgain();
  virtual bool searchUnsatSatMO();
  void search_MO() override;
  bool extendUL(uint64_t *upperObjv, uint64_t *upperObix);
  virtual bool extendUL(YPoint &ul);
  const std::set<Lit> &blocked_vars() { return blockedVars; };

protected:
  vec<Lit> explanation{}; // unsat explanation
  virtual bool rootedSearch(const YPoint &yp);
  std::set<Lit> blockedVars{};
  YPoint marker{};
};

} // namespace openwbo

#endif
#undef PARTIAL
#endif
