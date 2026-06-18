#ifndef ALG_PARPMINIMALMO_H
#define ALG_PARPMINIMALMO_H

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include "../Encoder.h"

#include "../MaxSAT.h"
#include "./Alg_ParallelMO.h"
#include "omp.h"
#include "utils/System.h"
#include <functional>
#include <map>
#include <set>
#include <syncstream>
#include <utility>

#define MAXDIM 50

namespace openwbo {

class ParPMinimalMO : public ParallelMO {

public:
  ParPMinimalMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_,
                int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_,
                int pb = _PB_SWC_, int pbobjf = _PB_GTE_, size_t nWorkers = 2,
                bool clausesharing = false, int apmode = encoding::_ap_outvars_,
                float eps = 1, int searchStrat = 3, float redFact = -1)
      : ParallelMO(verb, weight, strategy, enc, pb, pbobjf, nWorkers,
                   clausesharing, apmode, eps, searchStrat, redFact) {}

  ~ParPMinimalMO() {}

  void searchParPMinimalMO(size_t wid);
  void search_MO() override;
  void shareSolution(openwbo::Solution::OneSolution osol);

protected:
  vec<Lit> explanation; // unsat explanation
};
} // namespace openwbo

#endif
