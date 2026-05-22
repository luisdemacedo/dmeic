#ifndef ALG_MASTERMO_H
#define ALG_MASTERMO_H
#include "../partition.h"
#include "Alg_ServerMO.h"
#include <vector>

namespace openwbo {
class MasterMO {
public:
  PBtoCNFServerMO *optim;

public:
  virtual void initializeOptimizer(Solver *, MaxSATFormula *) = 0;
  // issues solve on the optim auxiliar solver
  virtual StatusCode compute_approx() { return optim->searchAgain(); }
  // changes data according to the results of compute_approx, stored
  // in solution
  virtual bool incorporate_approx() = 0;
  // get optim ready for the next solve call. If there is no need to
  // go on, return false.
  virtual bool setup_approx() = 0;
};

class PBtoCNFMasterMO : public virtual PBtoCNF, public virtual MasterMO {
public:
  bool buildWorkFormula() override;
  virtual void build() override = 0;
  void search_MO() override {
    build();
    if (firstSolution()) {
      buildWorkFormula();
      auto res = searchMasterMO();

      consolidateSolution();
      if (res == _OPTIMUM_ || res == _UNSATISFIABLE_)
        if (solution().size() == 0)
          answerType = _UNSATISFIABLE_;
        else
          answerType = _OPTIMUM_;
      else
        answerType = res;
      if (getStopSearchFlag()) {
        answerType = _INTERRUPTED_;
        DLOG(stderr,
             "%sstopSearch has been set to true, another thread requested to "
             "stop the search. Search stopped.\n",
             getSolverId().c_str());
        if (!_stopSearch)
          printAnswer(answerType);
        return;
      }

    } else
      answerType = openwbo::_UNSATISFIABLE_;

    requestStopSearch();
    shareSolutions(true);
    if (!_stopSearch)
      printAnswer(answerType);
  }

  StatusCode searchMasterMO() {
    auto res = _UNKNOWN_;
    do {
      res = compute_approx();
      if (getStopSearchFlag()) {
        DLOG(stderr,
             "%sstopSearch has been set to true, another thread requested to "
             "stop the search. Stopping search now...\n",
             getSolverId().c_str());
        return answerType;
      }

      incorporate_approx();
      if (res == _BUDGET_)
        return res;

      shareClauses();
      shareSolutions(true);
    } while (setup_approx());
    return res;
  }

  void
  setStopSearchFlag(std::shared_ptr<std::atomic<bool>> stopSearch) override {
    PBtoCNF::setStopSearchFlag(stopSearch);
    optim->setStopSearchFlag(stopSearch);
  }

  void setClausesBag(
      std::shared_ptr<clausesharing::ISharedClausesBag> bag) override {
    PBtoCNF::setClausesBag(bag);
    optim->setClausesBag(bag);
  }

  void setSharedSolutionsSet(
      std::shared_ptr<solutionsharing::ISharedSolutionsSet> set) override {
    PBtoCNF::setSharedSolutionsSet(set);
    optim->setSharedSolutionsSet(set);
  }

  void setShareClauses(bool share) override {
    PBtoCNF::setShareClauses(share);
    optim->setShareClauses(share);
  }

  void setClauseSharingHeuristic(
      clausesharing::IClauseSharingHeuristic *heuristic) override {
    PBtoCNF::setClauseSharingHeuristic(heuristic);
    optim->setClauseSharingHeuristic(heuristic);
  }

  void blockDominatedRegion(const YPoint &yp) override {
    optim->applyBlockDominatedRegion(yp);
  }

  void interruptSolver() override { optim->interruptSolver(); }
};
} // namespace openwbo
#endif
