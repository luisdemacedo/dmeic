#ifndef ALG_PARALLELMO_H
#define ALG_PARALLELMO_H

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include "../Encoder.h"

#include "../MaxSAT.h"
#include "./Alg_PBtoCNF.h"
#include "omp.h"
#include "utils/System.h"
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "../encodings/RootLits.h"
#include "../solutionsharing/ISharedSolutionsSet.h"

#define MAXDIM 50

namespace openwbo {

class ParallelMO : public MOCO {

public:
  using rootLits_t = std::shared_ptr<rootLits::RootLitsInt>;
  using invRootLits_t = std::shared_ptr<std::map<int, int>>;

  ParallelMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_,
             int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_,
             int pb = _PB_SWC_, int pbobjf = _PB_GTE_,
             int apmode = encoding::_ap_outvars_, float eps = 1,
             int searchStrat = 3, float redFact = -1)
      : cardinality_encoding(enc), pb_encoding(pb),
        pb_objective_encoding(pbobjf), approxMode(apmode), epsilon(eps),
        redFactor(redFact), answerType(_UNKNOWN_) {}

  ~ParallelMO() {
    for (auto &worker : workers)
      delete worker.solver;
  }

  // void search_MO() override;

  // StatusCode search() override;

  void printStats();
  bool updateMOFormulationIfSAT(size_t wid);
  bool updateMOFormulation(size_t wid);
  void init();
  bool firstSolution(); // Sets the worker's first solution and
                        // returns true if one is found.

protected:
  class Worker {
  public:
    Solver *solver = nullptr;
    vec<Lit> assumptions;

    encoding::Encoder encoder;
    encoding::GTE gtes[MAXDIM];
    encoding::KPA kps[MAXDIM];

    std::vector<rootLits_t>
        objRootLits; // (value, lit). lit => f < value, ~(f >= value)
    invRootLits_t invObjRootLits = std::make_shared<std::map<int, int>>();

    int nConflicts = -1;
    int nreencodes = 0;
    uint64_t fubs[MAXDIM] = {};

    Solution::OneSolution first{};
  };

  std::vector<Worker> workers;

  void initWorkers(size_t n) {
    // workers.resize(n);
    for (auto &worker : workers) {
      worker.solver = newSATSolver();
      worker.encoder.setCardEncoding(cardinality_encoding);
      worker.encoder.setPBEncoding(pb_objective_encoding);
      worker.nConflicts = conflict_limit;
      worker.objRootLits.clear();
      for (int i = 0; i < getFormula()->nObjFunctions(); i++)
        worker.objRootLits.push_back(
            std::make_shared<rootLits::RootLits>(rootLits::RootLits{}));
    }
  }

  bool enc_is_kp_based(size_t wid) {
    auto &encoder = workers[wid].encoder;
    return encoder.getPBEncoding() == _PB_KP_ ||
           encoder.getPBEncoding() == _PB_KP_MINISATP_;
  }

  lbool solve(size_t worker_id);
  void updateMOEncoding(size_t worker_id);

  void blockDominatedRegion(size_t worker_id, const YPoint &yp);
  void blockDominatedRegion(size_t worker_id, uint64_t *objix, int nObj);
  void assumeDominatingRegion(size_t worker_id, const YPoint &yp);
  void assumeDominatingRegion(size_t worker_id, uint64_t *objix, int nObj);

  // Options
  bool _useAllVars = false;
  int cardinality_encoding;
  int pb_encoding;
  int pb_objective_encoding;
  int approxMode;
  int conflict_limit = -1;

  // Statistics
  int nbMCS = 0;

  // MCS Management
  uint64_t _maxWeight = 0;

  // Quasi-Bones
  vec<int> _soft_variables;
  vec<int> _assigned_true;
  vec<float> _varScore;

  // MO support
  float epsilon = 1;
  float redFactor = -1;

  StatusCode answerType = _UNKNOWN_;

  std::shared_ptr<solutionsharing::ISharedSolutionsSet> sharedSolutions;
};
} // namespace openwbo

#endif
