#include "PBtoCNF_debugger.h"
namespace debug{
  Solution::OneSolution PBtoCNF_debugger::one_solution(){
    s->solve();
    return Solution::OneSolution(&sol, Model{s->solver->model});
  }
  PBtoCNF_debugger make_PBtoCNF_debugger(PBtoCNF* so){
    return PBtoCNF_debugger{so};
  };
  
}
