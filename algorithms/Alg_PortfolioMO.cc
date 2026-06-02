#include "Alg_PortfolioMO.h"

namespace openwbo {

StatusCode PortfolioMO::search() {
#pragma omp parallel for
  for (size_t idx = 0; idx < _portfolio.size(); idx++) {
    _portfolio[idx]->search();
  }

  for (size_t idx = 0; idx < _portfolio.size(); idx++) {
    DLOG(stdout, "Answer from solver %zu: %u\n", idx,
         _portfolio[idx]->answerType);
    if (_portfolio[idx]->answerType != _INTERRUPTED_) {
      this->answerType = _portfolio[idx]->answerType;
    }
  }

  DLOG(stdout, "Answer from portfolio: %u\n", this->answerType);

  printAnswer(this->answerType);
  return this->answerType;
  // portfolio result
}

void PortfolioMO::search_MO() {
  assert(false && "PortfolioMO::search_MO() should not be called");
}

void PortfolioMO::interruptSolver() {
  for (auto &solver : _portfolio)
    solver->interruptSolver();
  this->answerType = _INTERRUPTED_;
  printAnswer(answerType);
}

} // namespace openwbo
