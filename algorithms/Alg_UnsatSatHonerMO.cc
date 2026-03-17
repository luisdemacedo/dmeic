#include "Alg_UnsatSatHonerMO.h"

void UnsatSatHonerMO::search_MO() {
  // insert origin
  build();
  answerType = _UNSATISFIABLE_;
  if(firstSolution()){
    updateMOFormulation();
    blockDominatedRegion(first.yPoint());
    lbs.safe_insert(YPoint(getFormula()->nObjFunctions()));
    while(extend()){
      tmp_block_map.clear();
      assumptions.clear();
      search_below();
      conflict_t conflict;
      solver->conflict.copyTo(conflict);
      sort_conflict(conflict, _ascend);
      optimize_lbs(conflict);
      // describe_core(solver->conflict);
      for(auto l_yp: tmp_block_map){
	solver->addClause(l_yp.first);
      }

    }
    consolidateSolution();
    answerType = _OPTIMUM_;
  }
  else
    answerType = openwbo::_UNSATISFIABLE_;
  printAnswer(answerType);
}

void UnsatSatHonerMO::search_below(){
  lbool sat{};
  for(auto el: lbs){
    auto l = mkLit(solver->newVar());	
    tmp_block_map[l] = el;
    tmpBlockDominatedRegion(el,l);
    assumptions.push(~l);
  }

  while( (sat = solve()) == l_True){
    Model m = make_model(solver->model);
    // Only block dominated region if m1 gets into the Solution
    if(solution().pushSafe(m)){
      // block region dominated by last point
      auto sol = solution().oneSolution();
      auto yp = sol.yPoint();
      // create slide variable for newly found solution
      printf("c o ");
      std::cout<< sol << std::endl;
      runtime = cpuTime();
      printf("c new optimal solution (time: %.3f)\n", runtime - initialTime);
      blockStep(yp);
    }
    else{
      auto sol = solution().wrap(m);
      std::cout<<"c block point already stored"<<endl;
      printf("c o ");
      std::cout<< sol << std::endl;
      blockStep(sol.yPoint());
    }
      
  }
  if(sat == l_Undef){
    cout << "c budget exhausted during search below" <<  endl;
    answerType = _BUDGET_;
  }


  // fix temporary variables used during slide, which are listed
  // in the assumptions.

}

bool UnsatSatHonerMO::extend(){
  if(!lbs.size())
    return false;
  bool extend = false;

  decltype(lbs) lbs1;
  YPoint yp1;
  for(auto yp: lbs){
    for(int i = 0; i < yp.size(); i++){
      const auto& ol = objRootLits[i];
      auto it = ol->at_key(yp[i]+1);
      if(it != ol->end()){
	it++;
	if(it == ol->end())
	  continue;
      }
      extend = true;
      yp1 = yp;
      yp1[i] = it->first - 1;
      lbs1.safe_insert(yp1);
    }
  }
  if(extend){
    lbs  = lbs1;
    std::cout <<"c extension of lbs\n";
    std::vector<int> sums;
    int sum = 0;
    for(const auto& el: lbs){
      for(const auto& ell: el)
	sum+=ell;
      sums.push_back(sum);
      sum = 0;
    }
    cout << " sums: " << sums[0];
    for(auto it = sums.begin()++, end = sums.end(); it!=end;it++)
      cout << ", " << *it;
    cout << endl;
  }
  return extend;
}

void UnsatSatHonerMO::blockAbove(const YPoint& yp, Lit l){
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

void UnsatSatHonerMO::optimize_lbs(conflict_t& conflict){
  if(!conflict.size()){
    cout << " c empty core" << endl;
    lbs.clear();
    return;
  }else cout << "c (" << conflict.size() << "/"<< lbs.size() << ")" << endl;
  auto m = conflict.size();
  auto complete = false;
  if(_constructive) complete = optimize_core_constructive(conflict);
  else complete = optimize_core_destructive(conflict);
  if(complete && conf.size() < m){
    cout << "c new minimal core";
    cout << " (" << conf.size() << "/"<< m << ")" << endl;
  }

  Lit lit;
  int iObj = 0;
  int nFork = 0;
  YPoint ypp;

  decltype(lbs) lbs1;
  const auto& end = tmp_block_map.end();
  for(auto il = conf.size()-1; il >=0; il--){
    lit = conf[il];
    auto it = tmp_block_map.find(lit);
    if(it!=end)
      lbs1.insert(it->second);
  }
  lbs = lbs1;
}

// encode 'at most one lit in lits'.  Use r[i] to deactivate
// restrictions related with literal conflict[i].
void UnsatSatHonerMO::less_than_clause(const conflict_t& lits, 
				       const conflict_t& rs){
  vec<Lit> clause;
  for(int i = 0, n = lits.size(); i < n; i++){
    for(int j = 0, n = lits.size(); j < n; j++){
      if(j != i){
	clause.clear();
	clause.push(~lits[i]);
	clause.push(~lits[j]);
	clause.push(rs[i]);
	clause.push(rs[j]);
	solver->addClause(clause);
      }
    }
  }
}

// the second argument is a stack of variables to be removed from the
// assumptions.  Assume they are sorted by preference.  This of course
// depends on how the optimization takes place.
bool UnsatSatHonerMO::optimize_core_constructive(conflict_t& conflict){
  bool ret = false;
  lbool sat{};
  auto r = mkLit(solver->newVar());
  assumptions.clear();
  // copy of conflict, to avoid issues with pass by reference
  conflict_t total;
  conflict.copyTo(total);
  // approximation of the core. Variables that are definitely part of
  // core
  conflict_t core;
  // variables that may still partake in core
  vec<int> left_over;

  // relaxation variables, one per literal in conflict.  Used to
  // deactivate the implications of less_than_clause.
  conflict_t rs;
  for(int i = 0, n = total.size(); i < n; i++){
    left_over.push(i);
    rs.push(mkLit(solver->newVar()));
    assumptions.push(~rs.last());
  }
  less_than_clause(total, rs);
  while(left_over.size()){
    while((sat = solve()) == l_True){
      Model m = make_model(solver->model);
      // Only block dominated region if m1 gets into the Solution
      if(solution().pushSafe(m)){
	// block region dominated by last point
	auto sol = solution().oneSolution();
	auto yp = sol.yPoint();
	// create slide variable for newly found solution
	printf("c o ");
	std::cout<< sol << std::endl;
	runtime = cpuTime();
	printf("c new suboptimal solution (time: %.3f)\n", 
	       runtime - initialTime);
	if(_block)
	  blockStep(yp);
      }
      // deal with the variable that was set by the solver.  This
      // variable corresponds to an exclusive contribution, and
      // therefore must be part of the core that is under construction.
      for(int i = 0, n = left_over.size(); i < n; i++){
	Lit x = total[left_over[i]];
	if(solver->modelValue(x)==l_True){
	  // x is no longer left over
	  left_over.remove(left_over[i]); n--; i--;
	  // relax true variable.  There should be only one, because
	  // of the less_than clause
	  assumptions.push(~x);
	  core.push(x);
	  // deal with counting relax variable r
	  Lit r;
	  for(int j = 0, m = total.size(); j < m; j++)
	    if(x == total[j]){
	      r = rs[j];
	      break;
	    }
	  // disable relaxation variable.  Could be either set to true
	  // or false.  Setting it to 1 makes sense, because it
	  // deactivates the clause.
	  assumptions.remove(~r);
	  // relax r of i
	  solver->addClause(r);
	  // do not relax i  
	  break;
	}
      }
    }  
    if(sat == l_Undef){
      cout << "c budget exhausted during core optimization" << endl;
      answerType = _BUDGET_;
      ret = false;
      goto end;
    }
    else{
      // only points with r in core are interesting.  Find the next
      // such point, or return otherwise.
      if(!left_over.size()){
	ret = true;
	goto end;
      }      // There is no exclusive contribution by any element of
      // left_over.  Therefore, we take the choice into our own hands,
      // and choose one point to relax.  
      Lit x;
      // remove variables from left over
      decltype(left_over) lo;
      for(int j = 0, n = left_over.size(); j < n; j++){
	auto xj = left_over[j];
	x = total[xj];
	auto r = rs[xj];
	for(int i = 0, m = solver->conflict.size(); i < m; i++)      
	  if(r == solver->conflict[i]){
	    lo.push(xj);
	    break;
	  }
	if(!lo.size() || (lo.size() && total[lo.last()] != x)){
	  // relax x forever
	  solver->addClause(x);
	  assumptions.remove(~x);

	  // relax r of x forever
	  solver->addClause(r);
	  assumptions.remove(~r);
	}
      }
      lo.copyTo(left_over);
      // there is no r variable in conflict
      if(!left_over.size()){
	ret = true;
	goto end;
      }
      else{
	// choose next point to remove forcefully
	auto x = total[left_over.last()];
	auto r = rs[left_over.last()];
	left_over.pop();
	solver->addClause(x);
	assumptions.remove(~x);
	// relax r of x forever
	solver->addClause(r);
	assumptions.remove(~r);
}
	
    }
  }
  end:
  core.copyTo(conf);
  for(int j = 0, n = left_over.size(); j < n; j++)
    conf.push(total[left_over[j]]);
  return ret;

}

// the second argument is a stack of variables to be removed from the assumptions.
bool UnsatSatHonerMO::optimize_core_destructive(conflict_t& conflict){
  lbool sat{};
  for(int i = 0, n = conflict.size(); i < n; i++){
    assumptions.clear();
    for(int j = 0; j < n; j++)  
      if(j != i)
	assumptions.push(~conflict[j]);
    if((sat = solve()) == l_True){
      Model m = make_model(solver->model);
      // Only block dominated region if m1 gets into the Solution
      if(solution().pushSafe(m)){
	// block region dominated by last point
	auto sol = solution().oneSolution();
	auto yp = sol.yPoint();
	// create slide variable for newly found solution
	printf("c o ");
	std::cout<< sol << std::endl;
	runtime = cpuTime();
	printf("c new suboptimal solution (time: %.3f)\n", runtime - initialTime);
	if(_block)
	  blockStep(yp);
      }      
    }
    else if(sat == l_Undef){
      cout << "c budget exhausted during core optimization" << endl;
      answerType = _BUDGET_;
      return false;
    }
    else{
      solver->conflict.copyTo(conf);
      return optimize_core_destructive(solver->conflict);
    }
  }
  return true;
}

void  UnsatSatHonerMO::sort_conflict(conflict_t& conflict, bool ascend){
  std::vector<pair<Lit, YPoint>> s;
  for(int i = 0, n = conflict.size(); i < n; i++)
    s.push_back({conflict[i], tmp_block_map[conflict[i]]});
  if(ascend)
    std::sort(s.begin(), s.end(), 
	    [](const pair<Lit, YPoint>& l1, const pair<Lit, YPoint>& l2 ) {
	      return pareto::hv_shift(l1.second, pareto::max) < pareto::hv_shift(l2.second, pareto::max);});
  else
    std::sort(s.begin(), s.end(), 
	    [](const pair<Lit, YPoint>& l1, const pair<Lit, YPoint>& l2 ) {
	      return pareto::hv_shift(l1.second, pareto::max) > pareto::hv_shift(l2.second, pareto::max);});
  conflict_t sorted;
  for(const auto& el: s)
    sorted.push(el.first);
  sorted.copyTo(conflict);
}

StatusCode UnsatSatHonerServerMO::searchAgain(){
  assumptions.clear();
  answerType = _UNKNOWN_;
  if(!lbs.size()){
    lbs.safe_insert(YPoint(getFormula()->nObjFunctions()));
    extend();
  }
  // there is a conflict that has yet to be optimized.
  if(conf.size()){
    /* optimize_lbs(conf); */
    if(answerType != _BUDGET_){
      conf.clear();
      extend();
      for(auto l_yp: tmp_block_map){
	solver->addClause(l_yp.first);
      }
      tmp_block_map.clear();
    } else return answerType;
  }
  do{
    assumptions.clear();
    search_below();
    if(answerType == _BUDGET_)
      break;
    cout << "c new core" << endl;
    solver->conflict.copyTo(conf);
    sort_conflict(conf, _ascend);
    optimize_lbs(conf);
    lbs.report();
    // describe_core(solver->conflict);
    if(answerType == _BUDGET_){
      break;
    }
    for(auto l_yp: tmp_block_map){
      solver->addClause(l_yp.first);
    }
    tmp_block_map.clear();
  }while(extend());

  if(answerType != _BUDGET_)
    if(solution().size() == 0) 
      answerType = _UNSATISFIABLE_;
    else answerType = _OPTIMUM_;
  return answerType;
}
  bool UnsatSatHonerServerMO::not_done() {
    if(answerType == _UNKNOWN_)
      return true;
    else
      return false;
  }
  //will propagate changes innoculated by Master.
  void UnsatSatHonerServerMO::increment(){
    if(answerType == openwbo::_BUDGET_)
      answerType = openwbo::_UNKNOWN_;
  }
  void UnsatSatHonerServerMO::printAnswer(int){
    std::cout<<"c UnsatSatHonerServerMO report"<<endl;
    solution().report();
    lbs.report();
    std::cout<<"c UnsatSatHonerServerMO report done"<<endl;
  }
