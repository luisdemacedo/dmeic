#include "debug.h"
#include <cstdint>

namespace debug{
  openwbo::Model make_model(const vec<lbool>& m, uint64_t n){
    vec<lbool> proj;
    for(auto i = 0 * n; i < n; i++)
      proj.push(m[i]);
    return openwbo::Model{m};
  }

}
