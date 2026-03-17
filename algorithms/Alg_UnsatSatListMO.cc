#include "Alg_UnsatSatListMO.h"

bool UnsatSatListMO::searchUnsatSatMO() {
  int nObj = getFormula()->nObjFunctions();
  YPoint ul(nObj);
  auto res = rootedSearch(ul);
  assumptions.clear();

  if(res)
    if (solution().size() == 0) {
      answerType=_UNSATISFIABLE_;
    } 
    else
      answerType = _OPTIMUM_;
  else
    answerType = _BUDGET_;
  // of a budget exhaustion
  return true;
}
bool UnsatSatListMO::rootedSearch(const YPoint& yp) {
  cout << "c rooted search\n";
  double runtime = cpuTime();
  lbool sat{};
  YPoint ul = yp;
  if(!waiting_list->size())
    return true;

 newHarvest:
  if(waiting_list->size()){
    ul = waiting_list->pop();
    cout <<"c " <<"fence " << ul << 
      " with upper hv=" << pareto::hv_shift(ul, pareto::max) << endl;
  }
  else
    return true;
//   if(!check(ul)) {
//     cout << "c point " << ul << " pruned" << endl; 
//     goto newHarvest;
// }

  cout<<"c new harvest. upperLimit: "<< ul <<endl;
  //rebuild the assumptions
  assumptions.clear();
  assumeDominatingRegion(ul);
  while( (sat = solve()) == l_True) {
    Model m = make_model(solver->model);
    // Only block dominated region if m1 gets into the Solution
    if(solution().pushSafe(m)){
      blockStep(solution().yPoint());
      auto yp = solution().yPoint();

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
    assumptions.clear();
    //block blocked vars. For instance, when doing a MSU3 search
    assumeDominatingRegion(ul);
  }
  if (sat == l_Undef){
    waiting_list->unpop(1);
    std::cout << "budget exhausted while dealing with ul =  "<< 
      ul <<std::endl;
    return false;
  }
  if(solver->conflict.size()){
     glide(ul);
     // prune(solver->conflict, ul);
  }
  else
    return true;
  goto newHarvest;
  
  return true;
}
bool UnsatSatListMO::soar(){
  return{};
}


bool UnsatSatListMO::glide(const YPoint& yp){
  int nObj=yp.size();
  int nFork = 0;
  uint64_t upperObjv[nObj];
  uint64_t upperObjix[nObj];

  for(uint64_t i = 0; i < yp.size(); i++){
    upperObjv[i] = yp[i]+1;
  }
  evalToIndex(upperObjv, upperObjix);
  YPoint ypp = yp;
  vec<Lit> conflict;
  Lit lit;
  int iObj;
  solver->conflict.copyTo(conflict);
  while(conflict.size() > 0){
    lit = conflict.last();
    conflict.pop();
    iObj = getIObjFromLit(lit);
    if(iObj > -1){
      if(upperObjix[iObj] + 1 < (*objRootLits[iObj]).size()){
	if(!nFork){
	  std::cout << "c fork {" << iObj;
	}
	else
	  std::cout << "," << iObj;
	nFork++;
	auto old = ypp[iObj];
	ypp[iObj] = (*objRootLits[iObj])[upperObjix[iObj] + 1].first - 1;
	waiting_list->insert(ypp);
	ypp[iObj] = old;
      }
    }
  }
  if(nFork)
    std::cout << "} " << nFork << '\n';
  return nFork > 0;

}
void UnsatSatListMO::increment(){};
void UnsatSatListMO::printAnswer(int){
  std::cout<<"c UnsatSatListMO report"<<endl;
  waiting_list->report();
  solution().report();
  std::cout<<"c UnsatSatListMO report done"<<endl;
}
void UnsatSatListMO::build() {
  YPoint yp(getFormula()->nObjFunctions());
  ls.insert(yp);
  waiting_list->insert(yp);
}
  // returns true if test is not trivially unsatisfiable.  Assumes
  // conflict is not empty.
bool UnsatSatListMO::prune(const vec<Lit>& conflict, YPoint yp){
  // use core to update the test data structures. The data
  // structures should be used to check if a newly fetched point is
  // to be drilled or not.
  std::vector<int> objs;
  vec<Lit> conf;
  conflict.copyTo(conf);

  auto nObj=yp.size();

  int iObj{};


  Lit lit;
  while(conf.size() > 0){
    lit = conf.last();
    conf.pop();
    iObj = getIObjFromLit(lit);
    if(iObj > -1){
      objs.push_back(iObj);
      yp[iObj] = objRootLits[iObj]->at_key(yp[iObj]+1)->first;
    }
  }

  if(ls.empty())
    {
      for(auto i: objs){
	YPoint yp1(nObj);
	yp1[i] = yp[i];
	ls.insert(std::move(yp1));
      }
    }
  else{
    decltype(ls) b1 {};
    // iterate through b
    for(auto& xb: ls){
      // update b using b'
      for(auto i: objs){
	YPoint yp1{xb};
	if(yp1[i]<yp[i])
	  yp1[i] = yp[i];
	b1.safe_insert(yp1);
      }
    }
    ls.swap(b1);
  }

  return ls.size();
}
bool UnsatSatListMO::check(const YPoint& yp){ // 
  if(ls.empty())
    return true;
  int i = 0;
  for(const auto& xb: ls){
    if(pareto::dominates(xb, yp))
      {
	cout << "c point " << yp
	     << " used because " << "it is dominated by " << xb <<
	  " after " << i << " in " << ls.size() << endl;
	return true;
      }else i++;
  }
  return false;
}
