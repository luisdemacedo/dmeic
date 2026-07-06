#include "waiting_list.h"
namespace waiting_list {
unique_ptr<WaitingListI> construct(int wl_type, bool lower, bool ascend) {

  Types wl_t{wl_type};
  switch (wl_t) {
  case Types::Stack:
    return make_unique<Stack>();
    break;
  case Types::Queue:
    return make_unique<Queue>();
  case Types::List:
    if (lower)
      if (ascend)
        return make_unique<List<true, true>>();
      else
        return make_unique<List<true, false>>();
    else if (ascend)
      return make_unique<List<false, true>>();
    else
      return make_unique<List<false, false>>();
    break;
  case Types::PriorityQueue:
    if (lower)
      if (ascend)
        return make_unique<PriorityQueue<true, true>>();
      else
        return make_unique<PriorityQueue<true, false>>();
    else if (ascend)
      return make_unique<PriorityQueue<false, true>>();
    else
      return make_unique<PriorityQueue<false, false>>();
    break;
  case Types::SyncedStack:
    return make_unique<SyncedStack>();
    break;
  case Types::SyncedQueue:
    return make_unique<SyncedQueue>();
  }

  return NULL;
}
} // namespace waiting_list
