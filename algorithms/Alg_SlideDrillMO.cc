#include "Alg_SlideDrillMO.h"
namespace openwbo {

  void SlideDrillMO::search_MO(){
    build();
    answerType = _UNSATISFIABLE_;
    if(firstSolution()){
      updateMOFormulation();
      blockDominatedRegion(first.yPoint());
      YPoint yp{};
      for(int i = 0, n = getFormula()->nObjFunctions(); i < n; i++)
	yp.push_back(getFormula()->getUB(i) - getFormula()->getLB(i));
      waiting_list->insert(yp);
      searchBoundHonerMO();
      consolidateSolution();
      answerType = _OPTIMUM_;
    }
    else
      answerType = openwbo::_UNSATISFIABLE_;
    printAnswer(answerType);
  }  
  bool SlideDrillMO::searchBoundHonerMO(){
    YPoint yp{};
    lbool sat{};
    // push origin into b,

    //drill
    while(answerType != _BUDGET_){
      if(!drill())
	break;
      if(slide()){
	if(!solver->conflict.size())
	  return true;
	// prune(solver->conflict, drill_marker);
      }
    }
    return true;
  }
  // focus region dominating drill point, through manipulation of assumptions
  bool SlideDrillMO::drill(){
    lbool sat{l_False};
    YPoint yp{};
    while(waiting_list->size()){    
      assumptions.clear();
      yp = waiting_list->pop();
	
      cout <<"c " <<"drill from " << yp << 
      " with hv=" << hv(yp) << endl;
      auto it = mem.find(yp);
      // assume dominating region, until the next drill takes place.
      PBtoCNF::assumeDominatingRegion(yp);
      if(it!= mem.end()){
	drill_marker  = yp;
	for(auto l: it->second.deps)
	  assumptions.push(~l);
      }

      // look for the first queued element that is not optimal
      if((sat = solve()) != l_False) break;
      else{
	// describe_core(solver->conflict);
	printf("c if sat, optimal solution (time: %.3f)\n", 
	       runtime - initialTime);
	if(!solver->conflict.size())
	  return false;
	// prune(solver->conflict, yp);
	std::cout<< "c o " << yp << std::endl;
      }
    } 
    assumptions.clear();
    if(sat == l_Undef){
      cout << "c budget exhausted during drill. Push " << 
      yp << " again" << endl;
      answerType = _BUDGET_;
      waiting_list->unpop(1);
      return false;
    }
    if(sat == l_False)
      return false;
    drill_marker = yp;
    return true;
  }
  bool SlideDrillMO::check(YPoint yp){
    if(us.empty() && ls.empty())
      return true;
    int i = 0;
    for(const auto& xa: us)
      if(pareto::dominates(yp, xa))
	{
	  cout << "c point " << 
	  yp <<" used because " << "it dominates " << xa << endl;
	  return true;
	}
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
  // returns true if test is not trivially unsatisfiable.  Assumes
  // conflict is not empty.
  bool SlideDrillMO::prune(const vec<Lit>& conflict, YPoint yp){
    // use core to update the test data structures. The data
    // structures should be used to check if a newly fetched point is
    // to be drilled or not.
    std::vector<int> objs;
    vec<Lit> conf;
    conflict.copyTo(conf);

    auto nObj=yp.size();

    int iObj{};
    std::vector<Lit> slide_vars{};

    // set up coordinates according to drill site
    YPoint yp_x (nObj);
    Lit lit;
    while(conf.size() > 0){
      lit = conf.last();
      conf.pop();
      iObj = getIObjFromLit(lit);
      if(iObj > -1){
	objs.push_back(iObj);
	yp_x[iObj] = objRootLits[iObj]->at_key(yp[iObj]+1)->first;
      }
	// find variables related to slide ores
      else slide_vars.push_back(lit);
    }

    // build up al, using dependencies stored in mem.
    decltype(us) al;
    for(auto dep: slide_vars)
      al.insert(slide_map[dep]);

    if(us.empty() && ls.empty())
      {
	us.swap(al);
	for(auto i: objs){
	  YPoint yp1(nObj);
	  yp1[i] = yp_x[i];
	  ls.insert(yp1);
	}
      }
    else{
      decltype(ls) b1 {};
      decltype(us) a1 {};


      // iterate through b
      for(auto& xb: ls){
	// update b using b'
	for(auto i: objs){
	  YPoint yp1{xb};
	  if(yp1[i]<yp_x[i])
	    yp1[i] = yp_x[i];
	  b1.safe_insert(yp1);
	}
	// update b using a'
	auto hvb = pareto::hv_shift(xb, pareto::max);
	for(auto& xa: al)
	  // if intersection is not empty
	  if(pareto::dominates(xb, xa)){
	    cout << "c " << "approximate xb="<< xb << 
	      ", xa=" << xa << " pair by ";
	    // if region dominated by xb is larger than region
	    // dominating xa,
	    if(pareto::hv_shift(xa, pareto::min) < hvb){
	      cout << "xa" << endl;
	      a1.safe_insert(xa);
	    }else{
	      cout << "xb" << endl;
	      b1.safe_insert(xb);
	    }
	  }
      }

      // iterate through a
      for(auto& xa: us){
	// update a using b'
	auto hva = pareto::hv_shift(xa, pareto::min);
	for(auto i: objs){
	  if(xa[i]>=yp_x[i]){
	    // if xa intersects xb, a b' element, then keep xb.
	    YPoint xb(nObj);
	    xb[i] = yp_x[i];
	    auto hvb = pareto::hv_shift(pareto::max, xb);
	    if(hva < hvb)
	      a1.safe_insert(xa);
	    else
	      b1.safe_insert(xb);
	    break;
	  }
	}
	// update a using a'
	for(auto& xal: al){
	  // build intersection point, and insert it always.
	  YPoint min(nObj);
	  for(int i  = 0; i< nObj; i++){      
	    min[i] = xa[i] > xal[i]? xal[i]: xa[i];
	  }
	  a1.safe_insert(min);
	}
      }
      us.swap(a1);
      ls.swap(b1);
    }

    return us.size() || ls.size();
  }
  // temporarily disable region dominating yp, so that the solver will slide
  bool SlideDrillMO::slide(){
    lbool sat{};
    auto n_it = mem.find(drill_marker);
    PBtoCNF::assumeDominatingRegion(drill_marker);
    if(n_it != mem.end()){
      std::cout << "c restart slide under " << drill_marker << endl;	
      // block region below slide produces 
      for(auto dep: n_it->second.deps)
	assumptions.push(~dep);
    }
    else
      {
	Node n {drill_marker};
	auto pair = mem.insert({drill_marker, n});
	n_it = pair.first;
      }
    Node& n {n_it->second};
    do {
      Model m = make_model(solver->model);
      // Only block dominated region if m1 gets into the Solution
      if(solution().pushSafe(m)){
	// block region dominated by last point
	auto sol = solution().oneSolution();
	auto yp = sol.yPoint();
	// create slide variable for newly found solution
	auto l = mkLit(solver->newVar());	
	n.deps.push_back(l);
	slide_map[l] = yp;
	waiting_list->insert(yp);
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
    }while( (sat = solve()) == l_True);
    // fix temporary variables used during slide, which are listed
    // in the assumptions.
    if(sat == l_Undef){
      std::cout << "c budget exhausted during slide.";
      if(!drill_marker.empty()){
	std::cout << " Push " << drill_marker << " again";
	waiting_list->unpop(0);
      }
      std::cout << endl;      
      answerType = _BUDGET_;
      return false;
    }
    // describe_core(solver->conflict);
    for(auto l: n.deps){
      solver->addClause(l);
    }
    mem.erase(drill_marker);
    return true;
  }
  void SlideDrillMO::asssumeIncomparableRegion(const YPoint& yp, Lit l){
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
  void SlideDrillMO::describe_core(const conflict_t& conf){
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
      slide_vars.size() << "/" << 
      mem.find(drill_marker)->second.deps.size();
    }
    if(nFork || slide_vars.size())
      std::cout <<endl;
    else
      std::cout << "weird core" << endl;
    return;
  }
  StatusCode SlideDrillServerMO::searchAgain(){
    assumptions.clear();
    answerType = _UNKNOWN_;
    auto s = searchBoundHonerMO();
    if(answerType != _BUDGET_)
      if(solution().size() == 0) 
	answerType = _UNSATISFIABLE_;
      else answerType = _OPTIMUM_;
    return answerType;
  }
  bool SlideDrillServerMO::not_done() {
    if(answerType == _UNKNOWN_)
      return true;
    else
      return false;
  }
  //will propagate changes innoculated by Master.
  void SlideDrillServerMO::increment(){
    if(answerType == openwbo::_BUDGET_)
      answerType = openwbo::_UNKNOWN_;
  }
  void SlideDrillServerMO::printAnswer(int){
    std::cout<<"c SlideDrillServerMO report"<<endl;
    if (answerType == openwbo::_UNKNOWN_)
      if(drill_marker.size())
	waiting_list->insert(drill_marker);
    waiting_list->report();
    solution().report();
    std::cout<<"c SlideDrillServerMO report done"<<endl;
  }
}
