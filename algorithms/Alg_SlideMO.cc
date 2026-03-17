#include "Alg_SlideMO.h"

  void SlideMO::search_MO(){
    build();
    answerType = _UNSATISFIABLE_;
    if(firstSolution()){
      updateMOFormulation();
      blockDominatedRegion(first.yPoint());
      searchSlideMO();
      consolidateSolution();
      answerType = _OPTIMUM_;
    }
    else
      answerType = openwbo::_UNSATISFIABLE_;
    printAnswer(answerType);
  }

bool SlideMO::searchSlideMO(){
  // inspired by SlideDrillMO. Dropped drill
  while(answerType != _BUDGET_){
    std::cout<< "c new slide session" << endl;
    if(slide()){
      if(!solver->conflict.size())
	return true;
      assumptions.clear();
    }
  }
  return true;

}
  // temporarily disable region dominating yp, so that the solver will slide
  bool SlideMO::slide(){
    lbool sat{};
    
    if(slide_map.size()){
      std::cout << "c restart slide";
      for(auto l_yp: slide_map)
	assumptions.push(~l_yp.first);
    }
    while((sat = solve()) == l_True){
      Model m = make_model(solver->model);
      // Only block dominated region if m1 gets into the Solution
      if(solution().pushSafe(m)){
	// block region dominated by last point
	auto sol = solution().oneSolution();
	auto yp = sol.yPoint();
	// create slide variable for newly found solution
	auto l = mkLit(solver->newVar());	
	slide_map[l] = yp;
	printf("c o ");
	std::cout<< sol << std::endl;
	runtime = cpuTime();
	printf("c new suboptimal solution (time: %.3f)\n", runtime - initialTime);
	// temporarily avoid region dominating last point
	std::cout << "c " << "slide from " << yp << endl;
	blockStep(yp);
	// add temporary clause, and set toggling variable through global
	// assumptions
	asssumeIncomparableRegion(yp, l);
	assumptions.push(~l);
      }
    }
    // fix temporary variables used during slide, which are listed
    // in the assumptions.
    if(sat == l_Undef){
      std::cout << "c budget exhausted during slide." << endl;
      answerType = _BUDGET_;
      return false;
    }
    // describe_core(solver->conflict);
    for(auto l_yp: slide_map){
      solver->addClause(l_yp.first);
    }
    slide_map.clear();
    return true;
  }


void SlideMO::asssumeIncomparableRegion(const YPoint& yp, Lit l){
  auto nObj=yp.size();
  YPoint yp1 = yp;
  uint64_t objix[nObj];
  evalToIndex(yp1, objix);
  vec<Lit> clause;
  for(YPoint::size_type di = 0; di < nObj; di++){
    int j = objix[di];
    if(j < (int) (*objRootLits[di]).size()){
      clause.push(~(*objRootLits[di]).at(j).second);
    }
  }
  clause.push(l);
  solver->addClause(clause);
}

void SlideMO::describe_core(const conflict_t& conf){
  cout << "c new core";
  if(!conf.size()){
    cout << " empty core" << endl;
    return;
  }else cout << " (" << conf.size() << "): ";
  Lit lit;
  int iObj = 0;
  int nFork = 0;
  YPoint ypp;
  vec<Lit> conflict;
  conf.copyTo(conflict);

  vec<Lit> slide_vars{};
  while(conflict.size() > 0){
    lit = conflict.last();
    conflict.pop();
    iObj = getIObjFromLit(lit);
    if(iObj > -1){
      if(!nFork){
	std::cout << " fork {" << iObj;
      }
      else{
	std::cout << "," << iObj;
      }
      nFork++;
    }
    else
      slide_vars.push(lit);
  }
  if(nFork)
    std::cout << "} " << nFork;
  if(slide_vars.size()){
    if(nFork)
      std::cout << ", ";
    std::cout << "complains of slide block: (";
    for(auto n = slide_vars.size(),i = 0; i < n; i++)
      std::cout << slide_map[slide_vars[i]]<< ", ";
    std::cout << ") " << 
	  slide_vars.size() ;

      }
      if(nFork || slide_vars.size())
	std::cout <<endl;
      else
	std::cout << "weird core" << endl;
      return;
    }
