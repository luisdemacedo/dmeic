#ifndef VIEWER_H
#define VIEWER_H
#include "../Model.h"
#include "../encodings/RootLitsInt.h"
namespace view{
  void view_rootLits(const rootLits::RootLitsInt& rl, const Model& m);
}
#endif
