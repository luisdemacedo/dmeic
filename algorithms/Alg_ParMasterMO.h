#ifndef ALG_PARMASTERMO_H
#define ALG_PARMASTERMO_H
#include "../partition.h"
#include "Alg_ParServer.h"
#include <vector>

namespace openwbo {
class ParMasterMO {
public:
  ParallelMOServer *optim;

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

class ParallelMasterMO : public virtual ParallelMO, public virtual ParMasterMO {
public:
  virtual bool buildWorkFormula() {
    updateMOFormulation(MASTER_WORKER_ID);
    return true;
  };
  virtual void build() = 0;
  void search_MO() override {
    build();
    if (firstSolution(MASTER_WORKER_ID)) {
      buildWorkFormula();
      auto res = searchMasterMO();

      consolidateSolution(MASTER_WORKER_ID);
      if (res == _OPTIMUM_ || res == _UNSATISFIABLE_)
        if (workers[MASTER_WORKER_ID].solutions.size() == 0)
          answerType = _UNSATISFIABLE_;
        else
          answerType = _OPTIMUM_;
      else
        answerType = res;

    } else
      answerType = openwbo::_UNSATISFIABLE_;

    printAnswer(answerType);
  }

  StatusCode searchMasterMO() {
    auto res = _UNKNOWN_;
    do {
      res = compute_approx();

      incorporate_approx();
      if (res == _BUDGET_)
        return res;

    } while (setup_approx());
    // nbSatCalls = optim->nbSatCalls;
    return res;
  }

  // void setInitialTime(double initialTime) override {
  //   PBtoCNF::setInitialTime(initialTime);
  //   optim->setInitialTime(initialTime);
  // }
  //
  // void setClauseSharingHeuristic(
  //     clausesharing::IClauseSharingHeuristic *heuristic) override {
  //   PBtoCNF::setClauseSharingHeuristic(heuristic);
  //   optim->setClauseSharingHeuristic(heuristic);
  // }
  //
  // void blockDominatedRegion(const YPoint &yp) override {
  //   optim->applyBlockDominatedRegion(yp);
  // }
  //

protected:
  static constexpr std::size_t MASTER_WORKER_ID = 0;
};
} // namespace openwbo
#endif
