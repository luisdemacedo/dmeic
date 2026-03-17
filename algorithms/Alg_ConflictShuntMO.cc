#include "Alg_ConflictShuntMO.h"
#include "Alg_HittingSetsServerMO.h"

bool ConflictShuntMO::setup_approx(){
  if(lower.get() && upper.get())
    optim = (optim == upper.get())? lower.get(): upper.get();
  optim->increment();
  return optim->not_done();
}
bool ConflictShuntMO::incorporate_approx(){
  while(optim->solution().size()){
    auto osol = optim->solution().oneSolution();
    auto bvar = optim->solution().note();
    optim->solution().pop();
    auto m = Model{osol.model()};
    if(solution().pushSafe(m, bvar, true, true)){
      if(bvar > 0)
	blockStep(solution().yPoint());
    }
  }
  return false;
}
void ConflictShuntMO::initializeOptimizer(Solver* s, MaxSATFormula* m){
  if(upper != NULL){
    upper->setFormula(getFormula());
    upper->setSolver(s);
    upper->setRootLits(objRootLits, invObjRootLits);
    upper->ConflictLimit(conflict_limit);
    upper->build();
  }
  if(lower != NULL){
    lower->setFormula(getFormula());
    lower->setSolver(s);
    lower->ConflictLimit(conflict_limit);
    lower->setRootLits(objRootLits, invObjRootLits);
    lower->build();
  }
}
void ConflictShuntMO::build(){
  PBtoCNF::build();
  initializeOptimizer(solver,getMaxSATFormula());
}
bool ConflictShuntMO::buildWorkFormula(){
  PBtoCNFMasterMO::buildWorkFormula();
  // if(lower.get() != NULL){
  //   lower->buildWorkFormula();
  //   return true;
  // }
  // if(upper.get() != NULL) upper->buildWorkFormula();
  return true;
}
void ConflictShuntMO::search_MO(){
      build();
      if(firstSolution()){
	buildWorkFormula();
	blockDominatedRegion(first.yPoint());
	auto res = searchConflictShuntMO();
	PBtoCNF::consolidateSolution();
	if(res ==  _OPTIMUM_ || res == _UNSATISFIABLE_)
	  if(solution().size() == 0) answerType = _UNSATISFIABLE_;
	  else answerType = _OPTIMUM_;
	else
	  answerType = res;

      }
      else
	answerType = openwbo::_UNSATISFIABLE_;
      printAnswer(answerType);
    }
StatusCode ConflictShuntMO::searchConflictShuntMO(){
  auto res = _UNKNOWN_;
  do{
    res = compute_approx();
    incorporate_approx();
    if(res != _BUDGET_)
      break;
  }while(setup_approx());
  return res;
}
void ConflictShuntMO::consolidateSolution(){
  incorporate_approx();
  PBtoCNF::consolidateSolution();
}
void ConflictShuntMO::printAnswer(int answerType){
  if(lower)
    lower->printAnswer(0);
  if(upper)
    upper->printAnswer(0);
  solution().report();
  MOCO::printAnswer(answerType);
}
//will propagate changes innoculated by Master.
void HittingSetsConflictServerMO::increment(){
  if(answerType == openwbo::_BUDGET_)
    answerType = openwbo::_UNKNOWN_;
}
