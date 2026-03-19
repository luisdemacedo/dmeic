#ifndef ALG_PORTFOLIO_MO_H
#define ALG_PORTFOLIO_MO_H

#include "../Encoder.h"
#include <atomic>

#include "../MaxSAT.h"
#include "./Alg_PBtoCNF.h"
#include "utils/Options.h"
#include "utils/System.h"
#include <map>
#include <omp.h>
#include <set>
#include <utility>

namespace openwbo {

class PortfolioMO : public PBtoCNF {

public:
  PortfolioMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_,
              int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_,
              int pb = _PB_SWC_, int pbobjf = _PB_GTE_,
              std::vector<PBtoCNF *> portfolio = {})
      : PBtoCNF(verb, weight, strategy, enc, pb, pbobjf,
                new std::atomic<bool>(false)),
        _portfolio(portfolio) {

    if (portfolio.empty()) {
      std::cerr << "Error: Portfolio is empty." << std::endl;
      exit(1);
    }
    for (auto &solver : _portfolio) {
      solver->setStopSearchFlag(getStopSearchFlag());
    }
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

protected:
  std::vector<PBtoCNF *> _portfolio;

  void setInitialTime(double initial) override {
    for (auto &solver : _portfolio) {
      solver->setInitialTime(initial);
    }
  }

  void loadFormula(MaxSATFormula *formula) override {
    _portfolio[0]->loadFormula(formula);
    for (size_t i = 1; i < _portfolio.size(); ++i)
      _portfolio[i]->loadFormula(formula->copyPBFormula());
  }

  void setPrintModel(bool model) override {
    for (auto &solver : _portfolio) {
      solver->setPrintModel(model);
    }
  }

  void setPrint(bool doPrint) override {
    for (auto &solver : _portfolio) {
      solver->setPrint(doPrint);
    }
  }

  void setPrintSoft(const char *file) override {
    for (auto &solver : _portfolio) {
      solver->setPrintSoft(file);
    }
  }

  void setMyOutputFiles(const char *file) override {
    for (auto &solver : _portfolio) {
      solver->setMyOutputFiles(file);
    }
  }
};
} // namespace openwbo

#endif
