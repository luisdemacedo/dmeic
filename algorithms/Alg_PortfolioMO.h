#ifndef ALG_PORTFOLIO_MO_H
#define ALG_PORTFOLIO_MO_H

#include "../Encoder.h"
#include <atomic>

#include "../DebugLog.h"
#include "../MaxSAT.h"
#include "../clausesharing/DequeSharedClausesBag.h"
#include "../clausesharing/ISharedClausesBag.h"
#include "../solutionsharing/ISharedSolutionsSet.h"
#include "./Alg_PBtoCNF.h"
#include "utils/Options.h"
#include "utils/System.h"
#include <map>
#include <omp.h>
#include <set>
#include <signal.h>
#include <utility>

namespace openwbo {

class PortfolioMO : public PBtoCNF {

public:
  PortfolioMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_,
              int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_,
              int pb = _PB_SWC_, int pbobjf = _PB_GTE_,
              std::vector<PBtoCNF *> portfolio = {},
              bool stopSearchFlag = false, bool shareClauses = false,
              bool shareSolutions = false)
      : PBtoCNF(verb, weight, strategy, enc, pb, pbobjf),
        _portfolio(portfolio) {

    if (portfolio.empty()) {
      std::cerr << "Error: Portfolio is empty." << std::endl;
      exit(1);
    }

    if (stopSearchFlag)
      _stopSearch = std::make_shared<std::atomic<bool>>(false);

    for (auto &solver : _portfolio)
      solver->setStopSearchFlag(_stopSearch);

    sharedLearntClauses =
        std::make_shared<clausesharing::DequeSharedClausesBag>(
            _portfolio.size());
    for (auto &solver : _portfolio) {
      solver->setClausesBag(sharedLearntClauses);
    }
    setShareClauses(shareClauses);

    setShareSolutions(shareSolutions);

    for (auto &solver : _portfolio)
      solver->setStopSearchFlag(_stopSearch);

    sharedSolutions = std::make_shared<solutionsharing::SharedSolutionVector>(
        _portfolio.size());
    setSharedSolutionsSet(sharedSolutions);
  }

  ~PortfolioMO() {}

  StatusCode search() override;
  void search_MO() override;
  const std::vector<PBtoCNF *> &getSolvers() const { return _portfolio; }

  virtual MaxSATFormula *getMaxSATFormula() override {
    if (_portfolio.empty()) {
      return nullptr;
    }
    return _portfolio[0]->getMaxSATFormula();
  }

  void interruptSolver() override;

  void setPrintModel(bool model) override { MOCO::setPrintModel(model); }

protected:
  void setInitialTime(double initial) override {
    MOCO::setInitialTime(initial);
    for (auto &solver : _portfolio) {
      solver->setInitialTime(initial);
    }
  }

  void loadFormula(MaxSATFormula *formula) override {
    MOCO::loadFormula(formula->copyPBFormula());
    for (size_t i = 0; i < _portfolio.size(); ++i)
      _portfolio[i]->loadFormula(formula->copyPBFormula());
  }

  void setPrint(bool doPrint) override {
    MOCO::setPrint(doPrint);
    for (auto &solver : _portfolio) {
      solver->setPrint(doPrint);
    }
  }

  void setPrintSoft(const char *file) override {
    MOCO::setPrintSoft(file);
    for (auto &solver : _portfolio) {
      solver->setPrintSoft(file);
    }
  }

  void setMyOutputFiles(const char *file) override {
    if (!file)
      return;
    // TODO: check memory leaks
    std::string tmp = std::string(file) + ".portfolio";
    PBtoCNF::setMyOutputFiles(strdup(tmp.c_str()));
    for (size_t thread_id = 0; thread_id < _portfolio.size(); ++thread_id) {
      tmp = std::string(file) + ".thr" + std::to_string(thread_id);
      _portfolio[thread_id]->setMyOutputFiles(strdup(tmp.c_str()));
    }
  }

  void setStopSearchFlag(std::shared_ptr<std::atomic<bool>> flag) override {
    for (auto &solver : _portfolio) {
      solver->setStopSearchFlag(flag);
    }
  }

  void setClausesBag(
      std::shared_ptr<clausesharing::ISharedClausesBag> bag) override {
    for (auto &solver : _portfolio) {
      solver->setClausesBag(bag);
    }
  }

  void setSharedSolutionsSet(
      std::shared_ptr<solutionsharing::ISharedSolutionsSet> set) override {
    for (auto &solver : _portfolio) {
      solver->setSharedSolutionsSet(set);
    }
  }

  void setShareClauses(bool share) override {
    PBtoCNF::setShareClauses(share);
    for (auto &solver : _portfolio)
      solver->setShareClauses(share);
  }

  void setShareSolutions(bool share) override {
    PBtoCNF::setShareSolutions(share);
    for (auto &solver : _portfolio)
      solver->setShareSolutions(share);
  }

  void printAnswer(int type) override;

  void printSolutions();

  void printStats() override;

private:
  std::vector<PBtoCNF *> _portfolio;
};
} // namespace openwbo

#endif
