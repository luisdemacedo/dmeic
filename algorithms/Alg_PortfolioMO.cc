#include "Alg_PortfolioMO.h"

namespace openwbo {

StatusCode PortfolioMO::search() {
#pragma omp parallel for
  for (size_t idx = 0; idx < _portfolio.size(); idx++) {
    if (omp_get_thread_num() != 3)
      sleep(3); // sleep for a while to let the first thread start the search
    _portfolio[idx]->search();
  }

  for (size_t idx = 0; idx < _portfolio.size(); idx++) {
    printf("Answer from solver %d: %zu\n", idx, _portfolio[idx]->answerType);
    if (_portfolio[idx]->answerType != _INTERRUPTED_) {
      this->answerType = _portfolio[idx]->answerType;
    }
  }
  printf("Answer from portfolio: %zu\n", this->answerType);
  return this->answerType;
  // TODO: combine the results (like picking the non-unknown one) into the
  // portfolio result
}

void PortfolioMO::search_MO() {
  assert(false && "PortfolioMO::search_MO() should not be called");
}
} // namespace openwbo
