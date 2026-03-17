#include "Pareto.h"
// point that tightly dominates sol


// please fill this with max value in objective space
openwbo::YPoint pareto::max{};
openwbo::YPoint pareto::min{};
uint64_t pareto::hv_total = 0;
openwbo::YPoint pareto::dominator(Solution& sol){
  int nObj = sol.maxsat_formula()->nObjFunctions();
  YPoint yp(nObj);
  if(sol.size() == 0)
    return yp;
  yp = (*sol.begin()).second.first.yPoint();
  for(auto& os: sol){
    auto& yp1 = os.second.first.yPoint();
    for(int i = 0; i < nObj; i++)
      if(yp[i] > yp1[i])
	yp[i] = yp1[i];
  }
  return yp;
}

bool pareto::dominates(Solution::OneSolution &osol_a, 
		       Solution::OneSolution &osol_b){
  if(osol_a.comparable(osol_b))
    return dominates(osol_a.yPoint(), osol_b.yPoint());
  throw std::runtime_error("cannot compare incomparable OneSolution pair");
}


bool pareto::dominates(const YPoint &ypa, const YPoint &ypb){
  assert(ypa.size() > 0 && ypa.size() == ypb.size());
  for(int i = 0, n = ypa.size(); i < n; i++)
    if(ypa[i] > ypb[i])
      return false;
  return true;
}

bool pareto::dominates(Solution &sol, Solution::OneSolution& osol){
  //no const iterator, evaluating yPoint changes OneSolution
  if(sol.size() == 0)
    return false;
  try{
    for(auto& el: sol)
      if(dominates(el.second.first,osol)){
	return true;
	std::cout << "c new " << osol << 
	  " dominated by " << el.second.first << std::endl;
      }
  } catch(std::runtime_error& e) {std::cout << e.what();}
  return false;
}
bool pareto::dominates(Solution &sol_a, Solution &sol_b){
  //no const iterator, evaluating yPoint changes OneSolution
  auto it_b = sol_b.begin();
  try{
    while(dominates(sol_a, (it_b++)->second.first));
  if(it_b == sol_b.cend())
    return true;
  } catch(std::runtime_error& e) {std::cout << e.what();}
  return false;
}

bool pareto::dominates(Solution &sol_a, const Model& m){
  Solution::OneSolution osol = sol_a.wrap(m);
  return dominates(sol_a, osol);
}

uint64_t pareto::hv(const YPoint& u, const YPoint& l)
{
  if(!pareto::dominates(l,u))
    return 0;
  uint64_t x = 1;
  for( auto n=l.size(), i = n * 0; i < n; i++)
    x *= u[i] - l[i];
  return x;
}

uint64_t pareto::hv_shift(const YPoint& l, const YPoint& u)
{
  if(!pareto::dominates(l,u))
    return 0;
  uint64_t x = 1;
  for( auto n=l.size(), i = n * 0; i < n; i++)
    x *= u[i] - l[i] + 1;
  return x;
}


bool pareto::hv_compare_less(const YPoint& yp, const YPoint& ypp){
  auto yp_hv =  static_cast<int64_t>(hv_shift(pareto::min, yp))
    - static_cast<int64_t>(hv_shift(yp, pareto::max));
  auto ypp_hv =  static_cast<int64_t>(hv_shift(pareto::min, ypp)) 
    - static_cast<int64_t>(hv_shift(ypp, pareto::max));
  return yp_hv < ypp_hv;
}

double pareto::epsilon(const YPoint& yp, const YPoint& yp1){
  assert(pareto::dominates(yp, yp1));
  double max = 1;
  double eps;
  for(int i = 0, n = yp.size(); i < n; i++){
    if(yp[i] == yp1[i]) continue;
    else if(yp[i]> 0) eps = (double)yp1[i]/(double)yp[i];
    else continue;
    if(eps > max)
      max = eps;
  }
  return max;
}
