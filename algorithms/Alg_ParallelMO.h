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
#include <float.h>
#include <fstream>
#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <utility>
#include <vector>

#include "../clausesharing/DequeSharedClausesBag.h"
#include "../clausesharing/ISharedClausesBag.h"
#include "../clausesharing/SizeHeuristic.h"
#include "../encodings/RootLits.h"
#include "../solutionsharing/ISharedSolutionsSet.h"

#define MAXDIM 50

class ParConflictShuntMO;

namespace openwbo {

class ParallelMO : public MOCO {
  friend class ::ParConflictShuntMO;

public:
  using rootLits_t = std::shared_ptr<rootLits::RootLitsInt>;
  using invRootLits_t = std::shared_ptr<std::map<int, int>>;

  ParallelMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_,
             int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_,
             int pb = _PB_SWC_, int pbobjf = _PB_GTE_, size_t nWorkers = 2,
             bool clausesharing = false, int apmode = encoding::_ap_outvars_,
             float eps = 1, int searchStrat = 3, float redFact = -1)
      : cardinality_encoding(enc), pb_encoding(pb),
        pb_objective_encoding(pbobjf), _shareClauses(clausesharing),
        approxMode(apmode), epsilon(eps), redFactor(redFact),
        answerType(_UNKNOWN_) {
    workers = std::vector<Worker>(nWorkers);
  }

  ~ParallelMO() {
    for (auto &worker : workers)
      delete worker.solver;
  }

  StatusCode search() override;
  virtual void search_MO() = 0;
  void printStats();
  bool updateMOFormulationIfSAT(size_t wid);
  bool updateMOFormulation(size_t wid);
  void init();
  bool firstSolution(size_t wid); // Sets the worker's first solution and
                                  // returns true if one is found.

  std::string getSolverId() override {
    return "[s" + std::to_string(omp_get_thread_num()) + "] ";
  }
  void printAnswer(int type) override;
  void printSolutions();
  void consolidateSolution(size_t wid);

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
    Solution solutions{nullptr};
    size_t _nb_encoded_vars_initial = 0;
    std::unique_ptr<clausesharing::IClauseSharingHeuristic> sharingHeuristic =
        std::make_unique<clausesharing::SizeHeuristic>();

    int nbSatisfiable = 0;
    int nbSatCalls = 0;
    int nbEncVars = 0;
    int nbEncClauses = 0;
    int nbEncRootVars = 0;
    int nbSatCalls1stSol = 0;
    int nbReencodes = 0;
    double time1stSol = 0;
    double initialTime = 0;
  };

  std::vector<Worker> workers;

  virtual void initWorkers() {
    for (auto &worker : workers) {
      worker.encoder.setCardEncoding(cardinality_encoding);
      worker.encoder.setPBEncoding(pb_objective_encoding);
      worker.nConflicts = conflict_limit;
      worker.objRootLits.clear();
      for (int i = 0; i < getFormula()->nObjFunctions(); i++)
        worker.objRootLits.push_back(
            std::make_shared<rootLits::RootLits>(rootLits::RootLits{}));
      worker.solutions = Solution(this);
    }
    sharedSolutions =
        make_unique<solutionsharing::SharedSolutionsArchive>(workers.size());
    sharedLearntClauses =
        make_unique<clausesharing::DequeSharedClausesBag>(workers.size());
  }

  bool enc_is_kp_based(size_t wid) {
    auto &encoder = workers[wid].encoder;
    return encoder.getPBEncoding() == _PB_KP_ ||
           encoder.getPBEncoding() == _PB_KP_MINISATP_;
  }

  void buildSolversMO();
  void setConflictLimit(int limit) {
    conflict_limit = limit;
#pragma omp parallel for
    for (size_t wid = 0; wid < workers.size(); wid++)
      workers[wid].nConflicts = limit;
  }
  lbool solve(size_t worker_id);
  void updateMOEncoding(size_t worker_id);

  void blockDominatedRegion(size_t worker_id, const YPoint &yp);
  void blockDominatedRegion(size_t worker_id, uint64_t *objix, int nObj);

  int blockStep(size_t worker_id, const YPoint &yp);

  int assumeDominatingRegion(size_t worker_id, const YPoint &yp);
  void assumeDominatingRegion(size_t worker_id, uint64_t *objix, int nObj);

  void shareSolutions(size_t wid, bool alsoPull);
  void shareClauses(size_t wid);
  bool getShareClauses() { return _shareClauses; }

  void evalToIndex(size_t wid, const YPoint &yp, uint64_t *objix);
  void evalToIndex(size_t wid, uint64_t *objv, uint64_t *objix);

  void printApproxRatio() override;
  void updateStats() override;

  // Options
  bool _useAllVars = false;
  int cardinality_encoding;
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
  int encoding;
  int pb_encoding;

  StatusCode answerType = _UNKNOWN_;

  std::unique_ptr<solutionsharing::ISharedSolutionsSet> sharedSolutions;
  bool _shareClauses;
  std::unique_ptr<clausesharing::ISharedClausesBag> sharedLearntClauses;
};
} // namespace openwbo

#endif
