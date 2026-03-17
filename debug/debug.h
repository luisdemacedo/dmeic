#ifndef DEBUG_H
#define DEBUG_H
#include "../MOCO.h"
#include <cstdint>
namespace debug{
  openwbo::Model make_model(const vec<lbool>& m, uint64_t nvars);
}

#endif
