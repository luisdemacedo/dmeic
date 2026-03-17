
// #define PARTIAL
#include <utility>
#ifndef PARTIAL

#include <iostream>
#include "Alg_HittingSetsMO.h"	
#include <algorithm>    // std::max

using namespace openwbo;
//using namespace NSPACE;
using NSPACE::toLit;

//this clones every variable in the solver. Make sure it is called
//accordingly
void HittingSetsMO::initializeOptimizer(Solver* solv, MaxSATFormula* mxf){
  auto& f = *mxf;
  for(int i = 0, n = getFormula()->nObjFunctions(); i < n; ++i){
    auto pb = getFormula()->getObjFunction(i);
    f.addObjFunction(pb);
  }
  for(int i = 0, n = getFormula()->nInitialVars(); i < n; ++i)
    f.newVar();
  {
    f.setInitialVars(f.nVars());
    optim->loadFormula(&f);
    optim->ConflictLimit(conflict_limit);
    optim->build();
    auto formula = optim->getFormula();
    int64_t min = 0, max = 0;
    for (int i = 0; i < formula->nObjFunctions(); i++){
      max = formula->getObjFunction(i)->ub();
      formula->setUB(i, max);
      formula->setTighterUB(i, max);
      min = formula->getObjFunction(i)->lb();
      formula->setLB(i, min);
      formula->setTighterLB(i, min);
    };
    formula->setFormat(_FORMAT_PB_);
  }
}

void HittingSetsMO::genLowerBoundSet(){
  optim->searchAgain();
}

bool HittingSetsMO::diagnose(Solution::OneSolution& osol, vec<Lit>& assmpts){
  diagnoses.emplace_back();
  auto& conflict = (--diagnoses.end())->clause();
  for(int i = 0, n = solver->conflict.size(); i<n;i++){
    //i.e, diagnoses clause satisfied iff conflict is hit
    Lit lit = solver->conflict[i];
    conflict.push_back(lit);
    assmpts.remove(~lit);
  }
  return true;
}

bool HittingSetsMO::absorb(Solution::OneSolution& osol, int bvar){
  {
      auto m = Model{osol.model()};
      //removes elements of solution that are dominated by m.
      if(solution().pushSafe(m, bvar, true, true)){
	auto yp = solution().yPoint();
	cout <<"c o " << yp << endl;
	auto runtime = cpuTime();
	printf("c new optimal solution (time: %.3f)\n", runtime - initialTime);
      }
    }
 return true;
}

//saves sat lower bound set
bool HittingSetsMO::recycleLowerBoundSet(){
  const int nVars = getFormula()->nInitialVars();
  
  vec<Lit> assmpts{getFormula()->nInitialVars()};
  vec<Lit> partial_assmpts{};
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
      std::cout<<"c solution " << osol<< " not satisfiable\n";
      optim->mark_solution(id);
      diagnose(osol, assmpts);
    }
    else {return false;}
  }
  return andf;
}


void HittingSetsMO::incrementFormula(){
  cout << "diagnoses size: " << diagnoses.size() << endl;
  set<Lit> slice;

  for(auto& diag: diagnoses){
    for(auto& el: diag.second)
      slice.insert(el);
    vec<Lit> vecDiag(diag.clause().size());
    vectorVec(diag.clause(), vecDiag);
    optim->getSolver()->addClause(vecDiag);
  }

  optim_sliced->thaw(slice);
  optim->checkSols();
}
bool HittingSetsMO::setup_approx(){
  if(!diagnoses.size())
    return false;
  incrementFormula();
  diagnoses.clear();
  optim->increment();
  return optim->not_done();
}

bool HittingSetsMO::incorporate_approx(){
  consolidateSolution();
  return true;
}

void HittingSetsMO::consolidateSolution(){
  recycleLowerBoundSet();  
  PBtoCNF::consolidateSolution();
}

void HittingSetsMO::vectorVec(const std::vector<Lit>& vector, vec<Lit>& vec){
  for(int i = 0, n = vec.size();  i < n; i++)
    vec[i] = vector[i];
}
void HittingSetsMO::build(){
  PBtoCNF::build();
  MaxSATFormula* f = new MaxSATFormula{};
  initializeOptimizer(NULL, f);
}
bool HittingSetsMO::buildWorkFormula(){
  return optim->buildWorkFormula();
}

#endif
#undef PARTIAL

