#include "Alg_UnsatSatEpsilonDecayMO.h"
#include "Alg_UnsatSatEpsilonMO.h"
#include <iomanip>

namespace epsilon{

void UnsatSatEpsilonDecayMO::search_MO() {
  // insert origin
  build();
  answerType = _UNSATISFIABLE_;
  if(firstSolution()){
    updateMOFormulation();
    blockDominatedRegion(first.yPoint());
    lbs.safe_insert(YPoint(getFormula()->nObjFunctions()));
    while(geo_ratio >=1)
      {    
	std::cout<<"c ratio is " << geo_ratio << endl;
	while(extend()){
	  lit_to_point.clear();
	  point_to_lit.clear();
	  assumptions.clear();
	  for(auto& el: soft_blocks)
	    assumptions.push(~el.second);
	  search_below();
	  lbs = fence;
	  conflict_t conflict;
	  solver->conflict.copyTo(conflict);
	  sort_conflict(conflict, _ascend);
	  optimize_lbs(conflict);
	  // conf stores the best core seen so far, stored in conflict
	  conflict.copyTo(conf);
	  // describe_core(solver->conflict);
	  for(auto l_yp: lit_to_point){
	    solver->addClause(l_yp.first);
	  }
	  if(lbs.size())
	    lbs.report();
	  else report_e_sols(e_gbs_t);

	}
	if(geo_ratio==1)
	  break;
	geo_ratio /= 2;
	if(geo_ratio < 1)
	  geo_ratio = 1;

	assumptions.clear();
	for(auto& el: soft_blocks)
	  solver->addClause(el.second);
	soft_blocks.clear();

	decltype(e_gbs_t) e_gbs_t1;
	for(auto& el: e_gbs_t){
	  auto eps = pareto::epsilon(el.first, el.second);
	  if(eps < geo_ratio){
	    auto cnr = extend_point(el.first);
	    block_point_below(cnr);
	    // below_soft is filled by block_point_below
	  }else lbs.safe_insert(el.first);
 
	}
	lbs.report();
      };
    
    consolidateSolution();
    answerType = _OPTIMUM_;
  }
  else
    answerType = openwbo::_UNSATISFIABLE_;
  printAnswer(answerType);
}
// search below the current extended lower bound set, until it becomes
// a lower bound set once more.
void UnsatSatEpsilonDecayMO::search_below(){
  lbool sat{};
  for(auto el: fence){
    auto l = mkLit(solver->newVar());	
    lit_to_point[l] = el;
    point_to_lit[el] = l;
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
      blockStep(yp);
      bool imp = false;
      if(_drill){
	vec<Lit> assmpts;
	assumptions.copyTo(assmpts);
	toggleConflictBudget(conflict_budget);
	imp = drill(yp);
	toggleConflictBudget();
	assmpts.copyTo(assumptions);
	runtime = cpuTime();
      }
      if(_block_below){
	if(imp){
	  printf("c new optimal solution (time: %.3f)\n", runtime - initialTime);
	  block_region_below(yp);
	}
	else{
	  printf("c new e-optimal solution (time: %.3f)\n", runtime - initialTime);
	  block_region_below(yp);
	}      
      }
      else printf("c new solution (time: %.3f)\n", runtime - initialTime);
    }
    else {
      auto yp = MOCO::evalModel(m);
      for(auto& el: solution()){
	auto yp1 = el.second.first.yPoint();
	if(pareto::dominates(yp1,yp)){
	  blockStep(yp1);
	  printf("c o ");
	  std::cout<< yp1 <<std::endl;
	  printf("c old solution (time: %.3f)\n", runtime - initialTime);
	}
      }
    }
      
  }
  if(sat == l_Undef){
    cout << "c budget exhausted during search below" <<  endl;
    answerType = _BUDGET_;
  }


  // fix temporary variables used during slide, which are listed
  // in the assumptions.

}
// collects the e-gbs set, taking in consideration the freshly found
// solution
bool UnsatSatEpsilonDecayMO::block_region_below(const YPoint& yp){
  auto nObj=yp.size();
  for(const auto& el: e_gbs){
    const YPoint& yp0 = el.first;
    const YPoint& yp1 = el.second;
    if(!(pareto::dominates(yp,yp1) && pareto::dominates(yp0,yp)))
      continue;
    // keep around e-cells that contain the newly found point.
    e_sol.insert(yp);
    // corner is replaced by the solution that was found.

    e_gbs_t.insert({yp0, yp});
    auto blocked = block_point_below(yp1);
    if(!blocked)
      continue;
    std::cout << "c block " << yp1 << " because of " << yp << endl;
    return true;
  }
  return false;
}
bool UnsatSatEpsilonDecayMO::block_point_below(const YPoint& yp){
  if(soft_blocks.count(yp))
    return false;

  auto nObj=yp.size();
  uint64_t objix[nObj];
  evalToIndex(yp, objix);
  vec<Lit> clause;
  auto r = mkLit(solver->newVar());
  soft_blocks[yp] = r;
  clause.push(r);
  for(YPoint::size_type di = 0; di < nObj; di++){
    int j = objix[di];
    if(j < (int) (*objRootLits[di]).size()){
      clause.push(~(*objRootLits[di]).at(j).second);
    }
  }
    solver->addClause(clause);
    assumptions.push(~r);
    return true;
}

// move the lower bound set forward, producing the fence and the
// corner.  The corner is a set of points that are an upper bound
// set.
bool UnsatSatEpsilonDecayMO::extend(){
  if(!lbs.size())
    return false;
  bool xt = false;
  decltype(e_gbs) e_gbs1;
  decltype(lbs) fence1;
  YPoint yp1;
  for(const auto& yp: lbs){
    YPoint cnr = extend_point(yp);
    xt = false;
    for(int i = 0; i < cnr.size(); i++){
      if(cnr[i]> yp[i]){
	xt = true;
	yp1 = yp;
	yp1[i] = cnr[i];
	fence1.safe_insert(yp1);
      }
}
    // insert the new gap region, only if at least one coordinate was
    // forked.
    if(xt)
      e_gbs1.insert({yp, cnr});
  }
  e_gbs = e_gbs1;
  fence.clear();
  for(auto& lb: fence1){
    bool keep = true;
    for(auto& osol: solution()){
      auto& yp = osol.second.first.yPoint();
      if(pareto::dominates(yp, lb)){
	keep = false;
	std::cout<<"c " << lb << " removed from fence by " << yp << endl;
	// // I will keep blocking above yp while there are new lb points
	// // above.  And I think this can happen multiple times, and I
	// // will therefore block the same region multiple times.  I
	// // think I should deactivate this.
	// if(!_block){
	//   std::cout << "c block " << yp << endl;
	//   blockDominatedRegion(yp);
	// }
	if(_block_below && block_region_below(yp)){
	  // yp was already found, before the search over the current
	  // e-cells.
	  printf("c old e-optimal solution (time: %.3f)\n", runtime - initialTime);
	}
	break;
      }}
    if(keep)
      fence.insert(lb);
  }
  std::cout <<"c extension of lbs\n";
  std::vector<int> sums;
  int sum = 0;
  for(const auto& el: fence){
    for(const auto& ell: el)
      sum+=ell;
    sums.push_back(sum);
    sum = 0;
  }
  if(fence.size()){
    cout << "c sums: " << sums[0];
    for(auto it = ++sums.begin(), end = sums.end(); it!=end;it++)
      cout << ", " << *it;
    cout << endl;
  }
  return xt;
}

// extend yp, using eps as the multiplicative factor
YPoint UnsatSatEpsilonDecayMO::extend_point(const YPoint& yp){
  YPoint cnr = yp;
  for(int i = 0; i < yp.size(); i++){
    const auto& ol = objRootLits[i];
    if(geo_ratio > 1){
      auto v = (geo_ratio) * yp[i];
      // +1 is really ugly, but necessary to appease the interface
      // of rootLits. 
      auto it = ol->at_key(std::ceil(v)+1);
      /* fork yp point in i direction only if next value exists */
      if(it == ol->end()){
	// use last value of function, in order to setup the corner
	// point.  Otherwise, points in the e-cell wouldn't get a
	// representative in the gap set.
	--it;            
	cnr[i] = it->first - 1;
	continue; 
      }
      if(v < it->first - 1)
	v = it->first - 1;
      if(v == yp[i]) v++;
      cnr[i] = v;
    }else{
      auto it = ol->at_key(yp[i]+1);	
      for(auto j = arith_shift; j > 0 && it != ol->end(); j--, it++);
      if(it == ol->end()){	// fallen off the earth's edge? Should
	--it;     // not occur.
	cnr[i] = it->first - 1;
	continue;        
	  
      }
      cnr[i] = it->first - 1;
    }
  }
  return cnr;
}



// optimize the current lower bound set, after receiving a core that
// proves the lbs is a lbs
void UnsatSatEpsilonDecayMO::optimize_lbs(conflict_t& conflict){
  if(!conflict.size()){
    cout << " c empty core" << endl;
    lbs.clear();
    return;
  }else cout << "c current core: (" << conflict.size() << "/"<< lbs.size() << ")" << endl;
  auto m = conflict.size();
  auto complete = false;
  toggleConflictBudget(conflict_core);
  if(_core_optim==2){
    set<Lit> done{};
    for(auto& el: soft_blocks)
      done.insert(el.second);
    complete = optimize_core_destructive(conflict, done);
  }
  toggleConflictBudget();
  if(complete && conflict.size() < m){
    cout << "c new minimal core";
    cout << " (" << conflict.size() << "/"<< m << ")" << endl;
  } else if(complete) cout << "c core was minimal already" << endl;

  Lit lit;
  int iObj = 0;
  int nFork = 0;
  YPoint ypp;

  decltype(lbs) lbs1;
  const auto& end = lit_to_point.end();
  for(auto il = conflict.size()-1; il >=0; il--){
    lit = conflict[il];
    auto it = lit_to_point.find(lit);
    if(it!=end)
      lbs1.insert(it->second);
  }
  lbs = lbs1;
}

// the second argument is a stack of variables to be removed from the
// assumptions.  Assume they are sorted by preference.  This of course
// depends on how the optimization takes place.
bool UnsatSatEpsilonDecayMO::optimize_core_constructive(conflict_t& conflict){
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
  auto clause = less_than_clause(total, rs);
  if(clause.size())
    solver->addClause(clause);
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
	if(_block)
	  blockDominatedRegion(yp);
	if(_block_below && block_region_below(yp)){
	  printf("c new e-optimal solution during core optimization (time: %.3f)\n", runtime - initialTime);
	} else 	printf("c new suboptimal solution (time: %.3f)\n", runtime - initialTime);

      }
      // deal with the variable that was set by the solver.  This
      // variable corresponds to an exclusive contribution, and
      // therefore must be part of the core that is under construction.
      for(int i = 0, n = left_over.size(); i < n; i++){
	  auto xj = left_over[i];
	  Lit x = total[xj];
	if(solver->modelValue(x)==l_True){
	  // x is no longer left over
	  cout << "c contribution from " << lit_to_point[x] << endl;
	  left_over.remove(xj); n--; i--;
	  // relax true variable.  There should be only one, because
	  // of the less_than clause
	  assumptions.push(~x);
	  core.push(x);
	  // deal with counting relax variable r
	  r = rs[xj];
	  // disable relaxation variable.  Could be either set to true
	  // or false.  Setting it to 1 makes sense, because it
	  // deactivates the clause.
	  // relax clauses of x:
	  assumptions.remove(~r);
	  solver->addClause(r);
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
	if(!lo.size() || (lo.size() && lo.last() != xj)){
	  // relax x forever, if the r variable was not in the core.
	  cout << "c contribution empty from " << lit_to_point[x] << endl;
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
	// relax x forever, if the r variable was not in the core.
	cout << "c contribution empty from chosen " << lit_to_point[x] << endl;
	solver->addClause(x);
	assumptions.remove(~x);
	// relax r of x forever
	solver->addClause(r);
	assumptions.remove(~r);
}
	
    }
  }
  end:
  core.copyTo(conflict);
  for(int j = 0, n = left_over.size(); j < n; j++)
    conflict.push(total[left_over[j]]);
  return ret;

}

// the second argument is a stack of variables to be removed from the assumptions.
bool UnsatSatEpsilonDecayMO::optimize_core_destructive(conflict_t& conflict,  set<Lit>& done){
  lbool sat{};
  for(int i = 0, n = conflict.size(); i < n; i++){
    assumptions.clear();
    if(done.count(conflict[i])) 
      continue;
    auto x = conflict[i];
    for(int j = 0; j < n; j++)  
      if(j != i) assumptions.push(~conflict[j]);
    if((sat = solve()) == l_True){
      done.insert(x);
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
	if(_block)
	  blockStep(yp);
	if(_block_below && block_region_below(yp)){
	  printf("c new e-optimal solution, core optimization (time: %.3f)\n", runtime - initialTime);
	} else 	printf("c new suboptimal solution (time: %.3f)\n", runtime - initialTime);
	cout << "c contribution from " << lit_to_point[x] << endl;

      }      
    }
    else if(sat == l_Undef){
      cout << "c budget exhausted during core optimization" << endl;
      answerType = _BUDGET_;
      return false;
    }
    else{
      solver->conflict.copyTo(conf);
      conf.copyTo(conflict);
      return optimize_core_destructive(conflict, done);
    }
  }
  return true;
}

void  UnsatSatEpsilonDecayMO::sort_conflict(conflict_t& conflict, bool ascend){
  std::vector<pair<Lit, YPoint>> s;
  for(int i = 0, n = conflict.size(); i < n; i++)
    if(lit_to_point.count(conflict[i]))
      s.push_back({conflict[i], lit_to_point[conflict[i]]});
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
  for(int i = 0, n = conflict.size(); i < n; i++)
    if(!lit_to_point.count(conflict[i]))    
      sorted.push(conflict[i]);
  sorted.copyTo(conflict);
}

void UnsatSatEpsilonDecayMO::printAnswer(int answerType){
  e_sol.report();
  MOCO::printAnswer(answerType);
}


bool UnsatSatEpsilonDecayMO::drill(YPoint& yp){
  double runtime = cpuTime();
  int nObj = maxsat_formula->nObjFunctions();
  assumptions.clear();
  for(auto& el: soft_blocks)
    assumptions.push(~el.second);
  PBtoCNF::assumeDominatingRegion(yp);
  auto sat = solve();
  for(;sat == l_True; sat = solve()) {
    Model m =make_model(solver->model);
    solution().pushSafe(m);
    auto sol = solution().oneSolution();
    yp = sol.yPoint();
    blockDominatedRegion(yp);
    printf("c o ");
    std::cout<< sol << std::endl;
    runtime = cpuTime();
    printf("c new feasible solution (time: %.3f)\n", runtime - initialTime);
    assumptions.clear();
    for(auto& el: soft_blocks)
      assumptions.push(~el.second);
    PBtoCNF::assumeDominatingRegion(yp);
  }
  if(sat == l_Undef){
    cout << "c budget exhausted during drill inside cell."<< endl;
    answerType = _BUDGET_;
    return false;
  }
  runtime = cpuTime();
  return true;
}

  void report_e_sols(std::multimap<YPoint, YPoint>& e_gbs_t){
    std::cout<<"c e-cells and e-sols"<<endl;
    std::cout<<"c size: "<<e_gbs_t.size()<<endl;
    for(auto& el: e_gbs_t)
      std::cout<<"c "<<el.first << " | " << el.second
	       << ", epsilon: " << std::setprecision(2)
	       << pareto::epsilon(el.first, el.second) <<endl;

  }

}
