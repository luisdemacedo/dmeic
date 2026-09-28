#ifndef ALG_PARHITTINGSETSMO_H
#define ALG_PARHITTINGSETSMO_H

#include "../Encoder.h"
#include "Alg_MasterMO.h"
#include "Alg_ParMasterMO.h"

#include "../MaxSAT.h"
#include "./Alg_PBtoCNF.h"
#include "./Alg_ServerMO.h"
#include "./Alg_ParUnsatSatIncHSMO.h"
#include "./Alg_UnsatSatMO.h"
#include "./Alg_UnsatSatMSU3MO.h"
#include "./Alg_UnsatSatStratMSU3MO.h"
#include "utils/System.h"
#include <map>
#include <set>
#include <utility>

#define MAXDIM 50

namespace openwbo {

class ParHittingSetsMO : public virtual ParallelMO, public virtual MasterMO {
  class diagnosis : public std::pair<int, std::vector<Lit>> {
  public:
    int &id() { return first; }
    std::vector<Lit> &clause() { return second; }
  };

public:
  ParHittingSetsMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_,
                   int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_,
                   int pb = _PB_SWC_, int pbobjf = _PB_GTE_,
                   size_t nWorkers = 2, bool clausesharing = false,
                   int conf_budget = -1, int optimizerWorkers = -1,
                   double optimizerStride = 1.0, int expansionOrder = 0)
      : ParallelMO(verb, weight, strategy, enc, pb, pbobjf, nWorkers,
                   clausesharing) {
    setConflictLimit(conf_budget);
    if (optimizerWorkers == 0)
      optim_sliced = new UnsatSatIncHSMO(verb, weight, strategy, enc, pb, pbobjf);
    else
      optim_sliced = new ParUnsatSatIncHSMO(
          verb, weight, strategy, enc, pb, pbobjf,
          optimizerWorkers < 0 ? nWorkers : optimizerWorkers, clausesharing,
          optimizerStride, expansionOrder);
    if (optimizerWorkers == 0)
      printf("c ParHS feasibility workers: %zu, optimizer: sequential\n",
             nWorkers);
    else
      printf("c ParHS feasibility workers: %zu, optimizer workers: %zu\n",
             nWorkers, optimizerWorkers < 0 ? nWorkers : size_t(optimizerWorkers));
    optim = optim_sliced;
  }

  ~ParHittingSetsMO() {
    if (optim != NULL)
      delete optim;
    optim = NULL;
  }

  struct CandidateSolution {
    int id;
    Model model;
    Solution::notes_t bvar;
  };

  void vectorVec(const std::vector<Lit> &vector, vec<Lit> &vec);
  void search_MO() override;
  void genLowerBoundSet();
  bool buildWorkFormula();
  void incrementFormula();
  bool absorb(size_t wid, CandidateSolution &csol);
  bool diagnose(const std::vector<vec<Lit>> &unsatCores);
  bool virtual recycleLowerBoundSet();
  void initializeOptimizer(Solver *solv, MaxSATFormula *mxf) override;
  void consolidateSolution() override;
  bool setup_approx() override;
  bool incorporate_approx() override;
  void build();

protected:
  std::vector<diagnosis> diagnoses{};
  UnsatSatIncHSMO *optim_sliced;
};
} // namespace openwbo

#endif
