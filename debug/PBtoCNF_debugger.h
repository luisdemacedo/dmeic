#ifndef DEBUG_H
#define DEBUG_H
#include "../algorithms/Alg_PBtoCNF.h"
#include "core/Solver.h"
#include <iterator>

namespace debug{
  // build one solution, given the value of the model 
  using namespace openwbo;
  class PBtoCNF_debugger{
    PBtoCNF* s;
    Solution sol;
  public:
    PBtoCNF_debugger(PBtoCNF* so):s{so}, sol{so}{}
    Solution::OneSolution one_solution();
  };
  PBtoCNF_debugger make_PBtoCNF_debugger(PBtoCNF* so);
  
}

#endif
