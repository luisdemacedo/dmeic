#include "Model.h"

openwbo::Model::Model(const vec<lbool>& total,
		      const std::map<uint64_t, uint64_t> intervals){
  _intervals = intervals;
  uint64_t size = 0;
  for(const auto& interval: _intervals)
    size += interval.second - interval.first;
  resize(size);
  int j = 0;
  for(const auto& interval: _intervals)
    for(auto i = interval.first; i < interval.second; i++){
      (*this)[j++]=total[i];
    }
 }
