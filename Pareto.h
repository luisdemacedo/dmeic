#ifndef PARETO_H
#define PARETO_H
#include "MOCO.h"
#include <iostream> 
#include <set>

namespace pareto{
  using namespace openwbo;
  extern YPoint max;
  extern YPoint min;
  extern uint64_t hv_total;
  
  bool dominates(Solution::OneSolution& osol_a, 
		 Solution::OneSolution& mb);
  bool dominates(const YPoint& ypa, const YPoint& ypb);
  bool dominates(Solution& sola, Solution& solb);
  bool dominates(Solution& sol, const Model& m);
  bool dominates(Solution& sol, Solution::OneSolution& osol);
  bool dominates(Solution& sol, YPoint& yp);
  double epsilon(const YPoint& yp, const YPoint& yp1);
openwbo::YPoint dominator(Solution& sol);
  uint64_t hv(const YPoint& l, const YPoint& u=pareto::max);
  uint64_t hv_shift(const YPoint& l, const YPoint& u=pareto::max);  
  // returns true if yp is more interesting then ypp 
  bool hv_compare_less(const YPoint& yp, const YPoint& ypp);

  template<class element_t, bool polarity=false>
  class ThinSet: public std::set<element_t> {
    using set =  std::set<element_t>;
  public:
    ThinSet(): set{} {}
    inline bool dominates(const YPoint& a, const YPoint& b){
      if constexpr (!polarity)
	return pareto::dominates(a, b);
      else
	return pareto::dominates(b, a);
    }
    
    std::pair<typename set::iterator, bool> safe_insert(const element_t& x){
      for(auto it=set::begin(), end=set::end(); it!=end;)
	if(dominates(*it, x))
	  return {end, false};
	else if(dominates(x, *it))
	  it = set::erase(it);
	else it++;
      return set::insert(x);
    }
    void report(){
      std::cout<<"c thin set statistics"<<endl;
      std::cout<<"c size: "<<this->size()<<endl;
      for(auto it=set::begin(), end=set::end(); it!=end;it++)
	std::cout<<"c "<<*it<<endl;
      }
  };

  template<class element_t, bool polarity=false>
  class ThinGapSet: public std::set<std::pair<element_t, element_t>> {
    using set =  std::set<std::pair<element_t, element_t>>;
  public:
    ThinGapSet(): set{} {}
    inline bool dominates(const YPoint& a, const YPoint& b){
      if constexpr (!polarity)
	return pareto::dominates(a, b);
      else
	return pareto::dominates(b, a);
    }
    
    std::pair<typename set::iterator, bool> safe_insert(const std::pair<element_t, element_t>& x){
      for(auto it=set::begin(), end=set::end(); it!=end;)
	if(dominates(it->second, x.second))
	  return {end, false};
	else if(dominates(x.second, it->second))
	  it = set::erase(it);
	else it++;
      return set::insert(x);
    }
    void report(){
      std::cout<<"c lower epsilon thin set statistics"<<endl;
      std::cout<<"c size: "<<this->size()<<endl;
      for(auto it=set::begin(), end=set::end(); it!=end;it++)
	std::cout<<"c "<<it->first << " | " << it->second <<endl;
    }
  };
}
#endif
