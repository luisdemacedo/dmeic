#ifndef ALG_PBTOCNF_H
#define ALG_PBTOCNF_H

#include <memory>
#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include "../Encoder.h"

#include "../MOCO.h"
#include "../Pareto.h"
#include "../encodings/RootLits.h"
#include "utils/System.h"
#include <atomic>
#include <map>
#include <set>
#include <utility>

#define MAXDIM 50
namespace debug {
class PBtoCNF_debugger;
}
namespace openwbo {
class PBtoCNF : public MOCO {
  friend class debug::PBtoCNF_debugger;
  friend class PortfolioMO;

public:
  using rootLits_t = std::shared_ptr<rootLits::RootLitsInt>;
  using invRootLits_t = std::shared_ptr<std::map<int, int>>;

  PBtoCNF(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_,
          int strategy = _WEIGHT_NONE_,
          // int limit = INT32_MAX,
          int enc = _CARD_MTOTALIZER_, int pb = _PB_SWC_, int pbobjf = _PB_GTE_,
          std::atomic<bool> *stopSearch = nullptr) {
    solver = NULL;
    verbosity = verb;

    nbCores = 0;
    nbSatisfiable = 0;
    nbSatCalls = 0;
    sumSizeCores = 0;

    nbMCS = 0;
    _smallestMCS = UINT64_MAX;
    _lbWeight = 0;
    //       _maxMCS = 30;
    _maxMCS = INT32_MAX;
    _maxSAT = false;
    _useCores = false;
    _useAllVars = false;
    //       conflict_limit = 100000;
    conflict_limit = INT32_MAX;
    nConflicts = conflict_limit;
    pb_encoding = pb;
    // needed in MO version
    //       encoding = enc;
    encoder.setCardEncoding(encoding);
    //     encoder.setPBEncoding(pb);
    encoder.setPBEncoding(pbobjf);
    answerType = _UNKNOWN_;
    nreencodes = 0;
  }
  // wrapper of init and buildSolverMO
  virtual void build();
  // Proceed with the encoding of the work formula. Assumes some
  // stuff is already initialized. For instance, the solver. This is
  // not used by BLS.
  virtual bool buildWorkFormula() { return true; };
  virtual bool loadWorkFormula();
  shared_ptr<MOCOFormula> workFormula() override;
  StatusCode search() override; // BLS search.
  void consolidateSolution() override;
  void setUseCores(bool c) { _useCores = c; }
  void setMaxSAT(bool m) { _maxSAT = m; }
  void setMaxMCS(int n) { _maxMCS = n; }
  void setAllVarUsage(bool all_bones) {
    _useAllVars = all_bones; // false: only use soft vars in quasibones. true:
                             // use all vars
  }
  void setConflictLimit(int limit) {
    conflict_limit = limit;
    nConflicts = conflict_limit;
  }
  void setObjRootLits(std::vector<rootLits_t> orlo) { objRootLits = orlo; };
  // return the quasi-bones
  vec<Lit> &getQuasiBones() { return _quasibones; }

  vec<float> &getVariablesScore() { return _varScore; }

  uint64_t getCost() { // Unweighted. TODO: Save weighted solution!
    return _smallestMCS;
  }

  void setStopSearchFlag(std::atomic<bool> *stopSearch) {
    _stopSearch = stopSearch;
  }

  std::atomic<bool> *getStopSearchFlag() { return _stopSearch; }

  void updateStats() override;
  // MO part encodes objective functions. Overwrites objRootLits
  void updateMOEncoding();
  // wrapper of updateMOEncoding that keeps track of the number of
  // reencodings.
  bool updateMOFormulation();
  // wrapper that procedes only if the formula is sat
  bool updateMOFormulationIfSAT();
  // no need to search if the formula is unsat. firstSolution checks
  // this and sets the 'first' variable
  bool firstSolution();
  void addAssumptMOFormulation(bool omitRHS = false);
  virtual int blockStep(const YPoint &yp);
  bool MCSSearchMO(bool permanentBlock = false);
  int toggleConflictBudget(int limit = -1);
  void getObjRootLits(uint64_t *objv, uint64_t *objix, uint64_t *exact_objix,
                      int nObj);
  int getIObjFromLit(Lit lit) {
    try {
      return invObjRootLits->at(var(lit));
    } catch (std::out_of_range const &e) {
      return -1;
    }
  }

  void getNewObjRootLitsAndBlock(uint64_t *objv, uint64_t *objix, int nObj);
  uint64_t getTighterUB(int di);
  uint64_t getTighterLB(int di);

  bool apObjectivesAreExact();

protected:
  // Rebuild MaxSAT solver
  //
  Solver *buildSolver();

  lbool solve(); // SAT solver call

  // Utils for model management
  //
  void saveModel(vec<lbool> &currentModel); // Saves a Model.
  void saveSmallestModel(
      vec<lbool> &currentModel); // Saves the smallest model found until now.

  //  initializes members that require the instance formula.  some
  // sort. Assumes formula was loaded. Initializes the objRootLits
  // vector. Initializes variables to be used to implement soft
  // clauses.
  void init();

  void initUndefClauses(vec<int> &undefClauses);

  // Core Management
  lbool identifyDisjointCores();
  uint64_t coreMinCost(int c);

  // PBtoCNF search
  //
  void basicSearch(int maxMCS, bool maxsat);
  void basicCoreBasedSearch(int maxMCS, bool maxsat);

  bool findNextMCS();
  bool findNextCoreBasedMCS();
  void addMCSClause(vec<int> &unsatClauses);
  void addBackboneLiterals(int softIndex);

  // Utils for printing
  //
  void printModel();         // Print the last model.
  void printSmallestModel(); // Print the best satisfying model.
  void printStats();         // Print search statistics.

  // Other utils
  bool satisfiedSoft(int i);

  // MO constraints are passed to the solver. In particular, soft
  //  clauses are made soft, using whatever toggling variables were
  //  made available to the soft clause.
  Solver *buildSolverMO();
  void printRootLit(int d);
  void printRootLitModel(int d);
  void getPoint(uint64_t *ix, int d, uint64_t *out);
  void fixLBpoint(uint64_t *lb, uint64_t *ptdom, int d);

  void tmpBlockDominatedRegion(uint64_t *objix, int nObj, Lit tmplit);
  void tmpBlockDominatedRegion(const YPoint &yp, Lit tmplit);
  void blockDominatedRegion(uint64_t *objix, int nObj);
  void blockDominatedRegion(const YPoint &yp);
  void assumeDominatingRegion(uint64_t *objix, int nObj);
  virtual int assumeDominatingRegion(const YPoint &yp);
  void activateLit(Lit tmplit);

  void forget_temp_clauses();

  virtual void search_MO() = 0;
  void evalModel(vec<lbool> &currentModel, uint64_t *objv, uint64_t *ap_objv);
  void evalToIndex(uint64_t *objv, uint64_t *objix);
  void evalToIndex(const YPoint &yp, uint64_t *objix);

  void update_assumpt_n_mcs(Lit mcsesBlockLit, uint64_t *objix,
                            uint64_t *lastobjix, int nObj);

protected:
  // ------------------------------------------------------------------- //

  bool enc_is_kp_based() {
    return encoder.getPBEncoding() == _PB_KP_ ||
           encoder.getPBEncoding() == _PB_KP_MINISATP_;
  }

  // Data Structures

  // SAT solver and MaxSAT database
  //
  Solver *solver; // SAT solver used as a black box.
  //     int verbosity;                                      // Controls the
  //     verbosity of the solver.

  // Options
  int _maxMCS;
  bool _maxSAT;
  bool _useCores;
  bool _useAllVars;
  int conflict_limit = -1;
  int old_conflict_limit = -1;
  int nConflicts = -1;
  int delta_conflicts = -1;
  // Core extraction
  //
  std::map<Lit, int> coreMapping; // Maps the assumption literal to the number
                                  // of the soft clause.
  vec<Lit> assumptions; // Stores the assumptions to be used in the extraction
                        // of the core.
  vec<int> _prevAssumptions;

  vec<vec<int>> _cores;
  vec<vec<int>> _coreSatClauses;
  vec<vec<int>> _coreUnsatClauses;
  vec<int> _satClauses;

  // Symmetry breaking
  //
  vec<int>
      indexSoftCore; // Indexes of soft clauses that appear in the current core.
  vec<vec<int>>
      softMapping; // Maps the soft clause with the cores where they appears.

  // Statistics
  //
  int nbCores;           // Number of cores.
  uint64_t sumSizeCores; // Sum of the sizes of cores.
  //     int nbSatisfiable;                                  // Number of
  //     satisfiable calls. int nbSatCalls; // Number of SAT solver calls.
  int nbMCS;

  // MCS Management
  uint64_t _maxWeight;
  uint64_t _smallestMCS;
  uint64_t _lbWeight;
  vec<lbool> _smallestModel;
  int _nMCS;

  // Quasi-Bones
  vec<int> _soft_variables;
  vec<int> _assigned_true;
  vec<Lit> _quasibones;
  vec<float> _varScore;

  // MO support
  encoding::Encoder encoder; // Interface for the encoder of constraints to CNF.
  int encoding;              // Encoding for cardinality constraints.
  int pb_encoding;
  encoding::GTE gtes[MAXDIM]; // max MAXDIM objectives
  //     IGTE igtes[MAXDIM]; //max MAXDIM objectives
  encoding::KPA kps[MAXDIM]; // max MAXDIM objectives
  // array of vectors, storing controlling variables for each
  // objective (value, lit). lit => f < value-1, ~(f >= value - 1).
  // I think the description below is wrong.
  std::vector<rootLits_t>
      objRootLits{}; //(value, lit). lit => f < value, ~(f >= value)
  // maps vars to objectives
  invRootLits_t invObjRootLits =
      make_shared<std::map<int, int>>(std::map<int, int>{});
  //     std::vector<uint64_t *> nondom;
  float epsilon;
  StatusCode answerType;
  //     Lit mcsesBlockLit;

  bool approxMode;
  bool improvePhase;
  int sstrategy; // search strategy
  //     uint64_t reductionFactor[MAXDIM];
  float redFactor;

  uint64_t fubs[MAXDIM];
  Solution::OneSolution first{};
  int nreencodes;

  std::atomic<bool> *_stopSearch;
};
void evalToIndex(const YPoint &yp, uint64_t *objix,
                 std::vector<PBtoCNF::rootLits_t> &objRootLits);
PBObjFunction filter(const PBObjFunction *pb, set<Lit> &vars);
} // namespace openwbo

#endif
