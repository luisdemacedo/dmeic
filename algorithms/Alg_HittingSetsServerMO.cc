#include "Alg_HittingSetsServerMO.h"

void HittingSetsServerMO::checkSols(){
  for(auto it = solution().begin(), end = solution().end();it != end;){
    Solution::OneSolution osol = it->second.first;
    Model mod = osol.model();
    int bvar = it->second.second;
    auto osol_n = Solution::OneSolution{&solution(),mod};
    if(osol.yPoint() != osol_n.yPoint()){
      it = solution().remove(it);
      blocking_vars.erase(bvar);
      solution().push(osol_n.model());
    }else
      ++it;
  }
}
void HittingSetsServerMO::bootstrap(const Solution &usol){
  return;

}

bool HittingSetsServerMO::recycleLowerBoundSet(){
  const int nVars = getFormula()->nInitialVars();
  
  vec<Lit> assmpts{getFormula()->nInitialVars()};
  std::set<Lit> vars;
  for(int i = 0, n = getFormula()->nObjFunctions(); i < n; i++){
    auto& lits = getFormula()->getObjFunction(i)->_lits;
    for(int j = 0, m = lits.size() ;j < m; j++)
      vars.insert(lits[j]);
  }
  assmpts.clear();
  for(auto& el: vars)
    assmpts.push(el);
  if(!assmpts.size())
    return false;
  bool andf = true;
  for(auto& el: optim->solution()){
    auto& osol = el.second.first;
    int id = el.first;
    Solution::notes_t bvar = el.second.second;
    //checks satisfiability of complete model
    modelClause(modelEmbed(osol.model(), nVars),assmpts);
    lbool sat = solver->solveLimited(assmpts);

    if(sat == l_True)
      absorb(osol, bvar);
    else if(sat == l_False){
      andf = false;
      optim->mark_solution(id);
      if(solver->conflict.size())
	diagnose(osol, assmpts);
    }
    else return false;
  }
  return andf;
}

void HittingSetsServerMO::consolidateSolution(){
  recycleLowerBoundSet();  
}

StatusCode HittingSetsServerMO::searchAgain(){
  answerType = _UNKNOWN_;
  return searchMasterMO();
}

bool HittingSetsServerMO::not_done(){
  if(answerType == openwbo::_UNKNOWN_)
    return true;
  else
    return false;
}
void HittingSetsServerMO::build(){
  init();
  nbMCS = 0;
  answerType = _UNKNOWN_;
  MaxSATFormula* f = new MaxSATFormula{};
  initializeOptimizer(solver, f);
}
//will propagate changes innoculated by Master.
void HittingSetsServerMO::increment(){}
