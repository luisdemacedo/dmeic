#ifndef ALG_SERVERMO_H
#define ALG_SERVERMO_H

#include "Alg_DynamicMO.h"
#include "Alg_ServerMO.h"

#ifndef PARTIAL

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include "../MaxSAT.h"
#include "./Alg_ParallelMO.h"

namespace openwbo {

class ParServerMO : public DynamicMO {
public:
  virtual void setSolver(size_t wid, Solver *sol) = 0;
  virtual Solver *getSolver(size_t wid) = 0;
  virtual StatusCode searchAgain() = 0;
  virtual bool not_done() { return true; }
  virtual void bootstrap(const Solution &sol) = 0;
  virtual ~ParServerMO() = default;
  // TOOD: check these two below, interface might be wrong
  virtual int setConflictLimit(int cf) = 0;
  virtual int ConflictLimit() = 0;
};

class ParallelMOServer : public virtual ParallelMO,
                         public ParServerMO,
                         public virtual Bounded {
public:
  ParallelMOServer(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_,
                   int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_,
                   int pb = _PB_SWC_, int pbobjf = _PB_GTE_,
                   size_t nworkers = 2, bool clausesharing = false)
      : ParallelMO(verb, weight, strategy, enc, pb, pbobjf, nworkers,
                   clausesharing) {}

  void setSolver(size_t wid, Solver *sol) override {
    if (wid >= workers.size()) {
      std::osyncstream(std::cerr)
          << "Worker ID " << wid << " is out of range for " << workers.size()
          << " workers" << std::endl;
      std::abort();
    }

    workers[wid].solver = sol;
  }

  Solver *getSolver(size_t wid) override {
    if (wid >= workers.size()) {
      std::osyncstream(std::cerr)
          << "Worker ID " << wid << " is out of range for " << workers.size()
          << " workers" << std::endl;
      std::abort();
    }

    return workers[wid].solver;
  }

  void setRootLits(size_t wid, decltype(ParallelMO::Worker::objRootLits) orlo,
                   decltype(ParallelMO::Worker::invObjRootLits) iorlo) {
    if (wid >= workers.size()) {
      std::osyncstream(std::cerr)
          << "Worker ID " << wid << " is out of range for " << workers.size()
          << " workers" << std::endl;
      std::abort();
    }

    workers[wid].objRootLits = orlo;
    workers[wid].invObjRootLits = iorlo;
  }

  void setFormula(shared_ptr<MOCOFormula> mff) {
    mf = mff;
    maxsat_formula = mff->maxsat_formula();
    ubCost = mf->getSumWeights();
  }

  virtual ~ParallelMOServer() = default;

  // used to transfer a maxsatFormula into the MOCOFormula. Use it at
  // the infancy of the solver, only once.
  void loadFormula(MaxSATFormula *mxf) override { MOCO::loadFormula(mxf); }

  int setConflictLimit(int cf) override {
    int old_cf = conflict_limit;
    conflict_limit = cf;
    for (auto &w : workers) {
      w.solver->setConfBudget(cf);
      w.nConflicts = cf;
    }
    return old_cf;
  }
};
} // namespace openwbo
#endif
#undef PARTIAL
#endif
