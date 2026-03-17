#include "Alg_PMinimalServerMO.h"

namespace openwbo {

bool PMinimalServerMO::searchPMinimalServerMO() {
  double runtime = cpuTime();
  auto ul_old = ul;
  auto sat =
    solve();
  for(; sat == l_True; sat = solve()) {
    ul = ul_old;
    for(;sat == l_True; sat = solve()){
      Model m =make_model(solver->model);
      solution().pushSafe(m);
      ul = solution().yPoint();
      blockDominatedRegion(ul);
      printf("c o ");
      std::cout<< ul <<std::endl;
      runtime = cpuTime();
      printf("c new feasible solution (time: %.3f)\n", runtime - initialTime);
      assumptions.clear();
      PBtoCNF::assumeDominatingRegion(ul);
    }
    if(sat == l_Undef) break;

    assumptions.clear();
    runtime = cpuTime();
    printf("c new optimal solution (time: %.3f)\n", runtime - initialTime);
    blockDominatedRegion(ul);
  }

  if(sat == l_Undef){
    cout << "c budget exhausted while fence is at " << ul << endl;
    answerType = _BUDGET_;
    return false;
  }

  if (solution().size() == 0) {
    answerType=_UNSATISFIABLE_;
    return false;
  }else{
    answerType = _OPTIMUM_;
  }
  return true;
}


  StatusCode PMinimalServerMO::searchAgain(){
    assumptions.clear();
    answerType = _UNKNOWN_;
    auto s = searchPMinimalServerMO();
    if(answerType != _BUDGET_)
      if(solution().size() == 0) 
	answerType = _UNSATISFIABLE_;
      else answerType = _OPTIMUM_;
    return answerType;
  }
//will propagate changes innoculated by Master.
void PMinimalServerMO::increment(){
  if(answerType == openwbo::_BUDGET_)
    answerType = openwbo::_UNKNOWN_;
}

bool PMinimalServerMO::not_done() {
  if(answerType == _UNKNOWN_)
    return true;
  else
    return false;
}

}
