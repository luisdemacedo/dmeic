#ifndef ALG_PARALLELMO_H
#define ALG_PARALLELMO_H

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include "../Encoder.h"

#include "../MaxSAT.h"
#include "./Alg_PBtoCNF.h"
#include "omp.h"
#include "utils/System.h"
#include <map>
#include <set>
#include <utility>

#define MAXDIM 50

namespace openwbo {

class ParallelMO : public MOCO {

public:
  ParallelMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_,
             int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_,
             int pb = _PB_SWC_, int pbobjf = _PB_GTE_,
             int apmode = encoding::_ap_outvars_, float eps = 1,
             int searchStrat = 3, float redFact = -1) {
    solvers = buildSolversMO(num_workers);
  }

  ~ParallelMO() {}

  // void search_MO() override;
  std::vector<Solver *> buildSolversMO(size_t num_solvers) {
    std::vector<Solver *> solvers(num_solvers);
    for (size_t i = 0; i < num_solvers; i++)
      Solver *S = newSATSolver();
    return solvers;
  }

  // StatusCode search() override;

  void printStats();

protected:
  size_t num_workers = omp_get_max_threads(); // TODO: make this a parameter
  std::vector<Solver *> solvers;
};
} // namespace openwbo

#endif
