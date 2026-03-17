#include "Alg_HittingSetsStratMO.h"
#include "Alg_MasterMO.h"
#include "Alg_StratMO.h"
//after, optimizer is ready to work 
void HittingSetsStratMO::initializeOptimizer(Solver* solv, MaxSATFormula* mxf) {
  optim->setFormula(getFormula());
  for(int i = 0,nObj = optim->getFormula()->nObjFunctions(); i < nObj; i++ ){
    optim->getFormula()->replaceObjFunction(i, std::make_unique<PBObjFunction>(PBObjFunction{}));
  }
  optim->setSolver(solv);
  optim->build();
  // objectives start out empty
  }

bool HittingSetsStratMO::incorporate_approx(){
  for(auto& el: optim->solution()){
    auto& osol = el.second.first;
    Solution::notes_t bvar = el.second.second;
    auto m = Model{osol.model()};
    solution().pushSafe(m, bvar, true, true);
    auto yp = solution().yPoint();
  }
  return false;
}
void HittingSetsStratMO::build() {
  StratMO::build();
  initializeOptimizer(solver, NULL);
}

void HittingSetsStratMO::incrementEncoding(partition::MyPartition::part_t &p, int i) {
  auto& el = objRootLits[i];
  auto slice = dynamic_cast<rootLits::RootLitsSliced*>(el.get());
  PBObjFunction new_pb = slice->slice(p); 
  optim->getFormula()->replaceObjFunction(i, make_unique<PBObjFunction>(new_pb));
  }
bool HittingSetsStratMO::buildWorkFormula(){
  Solver* tmp = solver;
  solver = optim_hs->optim->getSolver();
  auto res = StratMO::buildWorkFormula();
  solver = tmp;
	for(int i = 0; i < (int) objRootLits.size(); i++){
	  auto old = *dynamic_cast<rootLits::RootLits*>(objRootLits[i].get());
	  objRootLits[i] = 
	    std::make_shared<rootLits::RootLitsSliced>
	    (rootLits::RootLitsSliced{std::move(old), *getFormula()->getObjFunction(i)});
	}
  return res;
}
bool HittingSetsStratMO::setup_approx(){
  auto res = StratMO::setup_approx();
  return res;
}

//will propagate changes innoculated by Master.
void HittingSetsStratServerMO::increment(){
  checkSols();
  optim->solution().clear();
  optim->lowerBound.clear();
  optim->copyObjRootLits(invObjRootLits, objRootLits, 0);
  for(int i = 0, n = getFormula()->nObjFunctions(); i < n; i++){
    auto& new_pb = *getFormula()->getObjFunction(i);
    optim->getFormula()->replaceObjFunction(i, make_unique<PBObjFunction>(new_pb));
  }
  for(auto& el: solution())
    optim->solution().pushSafe(el.second.first.model(), el.second.second);
  nbMCS = 0;
  answerType = _UNKNOWN_;
  optim->increment();
}
