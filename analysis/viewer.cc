#include "viewer.h"

void view::view_rootLits(const rootLits::RootLitsInt &rl, const Model& m){
    for(auto i = rl.cbegin(), end = rl.cend(); i != end; i++){
      auto l = i->second;
      int x = var(l);
      printf("%lu->%d: x%d\n",i->first , m[var(l)] == l_True, x + 1);
    }
  }
