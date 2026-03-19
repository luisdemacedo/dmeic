#include "Alg_PortfolioMO.h"

namespace openwbo {

StatusCode PortfolioMO::search() {
#pragma omp parallel for
  for (size_t idx = 0; idx < _portfolio.size(); idx++)
    _portfolio[idx]->search();

  for (size_t idx = 0; idx < _portfolio.size(); idx++) {
    printf("Answer from solver %d: %zu\n", idx, _portfolio[idx]->answerType);
  }
  printf("Answer from portfolio: %zu\n", this->answerType);
  return this->answerType;
}

void PortfolioMO::search_MO() {
  assert(false && "PortfolioMO::search_MO() should not be called");
}
} // namespace openwbo
