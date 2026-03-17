#include "FormulaPB.h"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace openwbo{
  std::unique_ptr<PBObjFunction>  add(PBObjFunction const * pb, PBObjFunction const * pbb){
    vec<Lit> lits{};
    vec<uint64_t> coeffs{};
    std::map<Lit, uint64_t> terms{};  
  
    if(pb!=nullptr)
      for(int j = 0; j < pb->_lits.size(); j++)
	terms[pb->_lits[j]]+=pb->_coeffs[j]*pb->_factor;
    if(pbb!=nullptr)
      for(int j = 0; j < pbb->_lits.size(); j++)
	terms[pbb->_lits[j]]+=pbb->_coeffs[j]*pbb->_factor;
    for(auto& el: terms){
      lits.push(el.first);
      coeffs.push(el.second);
    }

    return std::make_unique<PBObjFunction>(PBObjFunction{lits,coeffs,pb->_const + pbb->_const});
  }
  
  bool PBObjFunction::compute_sparse(){
    int n = _lits.size();
    if(!n) return true;
    std::set<uint64_t>::reverse_iterator rit;
    std::vector<uint64_t> sorted(n);
    for(int i = 0; i < n; i++)
      sorted[i] = _coeffs[i];
    std::sort(sorted.begin(), sorted.end());
    auto upper = sorted[0];
    _sparse = false;
    for(int i = 1; i < n; i++){
      if(sorted[i] <= upper + 1){
	upper += sorted[i];
      }else {
	_sparse = true;
	break;
      }
    }
    return _sparse;
  }
}
