#include "Alg_PortfolioMO.h"

namespace openwbo {
void PortfolioMO::search_MO() {

#pragma omp parallel for
  for (size_t idx = 0; idx < _portfolio.size(); idx++) {
    _portfolio[idx]->search_MO();
  }
}
} // namespace openwbo
