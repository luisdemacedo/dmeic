#include "Alg_UnsatSatIncHSMO.h"

bool UnsatSatIncHSMO::searchUnsatSatMO() {
  int nObj = getFormula()->nObjFunctions();
  YPoint ul(nObj);
  if(!lowerBound.size())
    rootedSearch(ul);
  else{
    YPoint dom = pareto::dominator(lowerBound);
    if(pareto::dominates(ul,dom))
      ul = dom;
    rootedSearch(ul);
  }
  if (solution().size() == 0) {
    answerType=_UNSATISFIABLE_;
    return false;
  }else{
    answerType = _OPTIMUM_;
  }
  
  return true;
}

bool UnsatSatIncHSMO::rootedSearch(const YPoint& yp){
  cout << "c rooted search\n";
  double runtime = cpuTime();
  lbool sat{};
  YPoint ul = yp;

 newHarvest:
  cout<<"c new harvest. upperLimit: "<< ul <<endl;
  //block blocked vars. For instance, when doing a MSU3 search
    assumptions.clear();
    for(const auto& el: blockedVars)
      assumptions.push(~el);
    //block region dominated so far
    for(const auto& el: solution())
      assumptions.push(Glucose::mkLit(el.second.second, false));
  assumeDominatingRegion(ul);
  while( (sat = solve()) == l_True) {
    Model m = make_model(solver->model);
    // Only block dominated region if m1 gets into the Solution
    if(solution().pushSafe(m)){
      auto ix = blockStep(solution().yPoint());
      solution().note(-1) = ix;
      assumptions.push(mkLit(ix, false));
      auto yp = solution().yPoint();
      // update soft variables that are still relevant
      runtime = cpuTime();
      cout <<"c o " << yp << endl;
      printf("c new inner optimal solution (time: %.3f)\n", runtime - initialTime);
    }
    else {
      auto one = Solution::OneSolution{&solution(),m};
      cout <<"c o " << one.yPoint() << endl;
      runtime = cpuTime();
      printf("c new non-optimal solution (time: %.3f)\n", runtime - initialTime);
    }
  }
  if (sat == l_Undef){
    marker = ul;
    std::cout << "budget exhausted while dealing with ul =  "<< 
      marker <<std::endl;
    return false;
  }

  auto old = ul;
  if(extendUL(ul)){
    // prune(solver->conflict, old);
    goto newHarvest;
  }
  return true;
}


int UnsatSatIncHSMO::blockStep(const YPoint& yp){
  forceSlice(true);
  Lit x;
  if(soft_blocks.count(yp)){
    x = soft_blocks[yp];
  }else{
    x = mkLit(solver->newVar(), true);
    tmpBlockDominatedRegion(yp, x);
    soft_blocks[yp] = x;
  }
  toggleSlice();
  return Glucose::var(x);
}

void UnsatSatIncHSMO::checkSols() {
  lowerBound.clear();
  
  for(auto it = solution().begin(), end = solution().end();it != end;){
    // marked solutions should be moved onto the lower bound set.
    if(marked_sols.count(it->first)){
      lowerBound.push(std::move(it->second));
      it = solution().remove(it);
      continue;
    }
    // solution is sat in larger formula, and therefore it is optimal.
    // I can block it and erase every info on it
    int bvar = it->second.second;
    soft_blocks.erase(it->second.first.yPoint());
    blocking_vars.erase(bvar);
    // block point permanently
    solver->addClause(mkLit(bvar, false));
    it = solution().remove(it);
  }

  std::cout << "c lower bound: " << lowerBound.size() << endl;
  for(auto& el: lowerBound)
    std::cout << "c "  << el.second.first.yPoint() <<endl;
}
void UnsatSatIncHSMO::incrementSlice(const partition::MyPartition::part_t& p){
  int i = 0;
  for(const auto& x: p)
    if(blockedVars.count(x))
      blockedVars.erase(x);
  
  for(auto& el: objRootLits){
    auto sliced = dynamic_cast<rootLits::RootLitsSliced*>(el.get());
    sliced->slice(p);
    // objective function is updated whenever the slice is incremented
    getFormula()->replaceObjFunction(i++, 
				     std::make_unique<PBObjFunction>(PBObjFunction{sliced->cur}));
  }

}

void UnsatSatIncHSMO::thaw(const set<Lit>& s){
  set<Lit> s1{};
  int i = 0;
  for(const auto& x: s){
    s1.insert(x);
    if(blockedVars.count(x))
      blockedVars.erase(x);
    else
    s1.insert(~x);
  }
  for(auto& el: objRootLits){
    auto sliced = dynamic_cast<rootLits::RootLitsSliced*>(el.get());
    auto pb = sliced->thaw(s1);
    // objective function is updated whenever the slice is incremented
    getFormula()->replaceObjFunction(i++, std::make_unique<PBObjFunction>(pb));
  }

}

int UnsatSatIncHSMO::assumeDominatingRegion(const YPoint& yp) {
  forceSlice(true);
  int pushed = UnsatSatMO::assumeDominatingRegion(yp);
  toggleSlice();
  return pushed;
}

bool UnsatSatIncHSMO::extendUL(YPoint& ul){
  forceSlice(true);
  bool ret =  UnsatSatMO::extendUL(ul);
  toggleSlice();
  return ret;
}
const PBObjFunction& UnsatSatIncHSMO::slicedObjective(int i){
  auto sliced = dynamic_cast<rootLits::RootLitsSliced*>(objRootLits[i].get());
  return sliced->cur;
}
void UnsatSatIncHSMO::increment(){
}
