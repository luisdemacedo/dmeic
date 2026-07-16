#ifndef ALG_PARSLIDEDRILL_H
#define ALG_PARSLIDEDRILL_H
#include "utility"
#include <map>

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include "../Encoder.h"

#include "../MaxSAT.h"
#include "../waiting_list/waiting_list.h"
#include "./Alg_ParServer.h"
#include "./Alg_ParallelMO.h"
#include "./Alg_ServerMO.h"
#include "utils/System.h"
#include <utility>

namespace openwbo {

enum class DrillResult { FoundModel, UnsatNoCore, UnsatCore, Budget };
using conflict_t = vec<Lit>;
class ParSlideDrillMO : public virtual ParallelMO, public virtual Bounded {
  constexpr static bool polarity = true; // because it is a sat-unsat solver;
  class Node : public YPoint {           // TODO: to review
  public:
    Node() {}
    Node(YPoint &ypo) : YPoint{ypo} {}
    vector<Lit> deps{};
  };

public:
  ParSlideDrillMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_,
                  int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_,
                  int pb = _PB_SWC_, int pbobjf = _PB_GTE_, size_t nworkers = 2,
                  bool clausesharing = false,
                  int apmode = encoding::_ap_outvars_, float eps = 1,
                  int searchStrat = 3, bool ascend = false, bool lower = false,
                  int wl_type = 0)
      : ParallelMO(verb, weight, strategy, enc, pb, pbobjf) {
    waiting_list = waiting_list::construct(wl_type, lower, ascend);
  }

  ~ParSlideDrillMO() {}

  bool searchBoundHonerMO();
  void search_MO() override;
  bool slide(size_t wid);
  bool drill(size_t wid);
  DrillResult drillFromPoint(size_t wid, const YPoint &yp);
  // TODO: implement prune and check, as they are unused in the sequential
  // version
  //
  // bool prune(const vec<Lit>& conflict, YPoint yp);
  // bool check(YPoint yp);
  class SDWorkerState {
  public:
    YPoint drill_marker;
    // results of slide, and corresponding blocking variables
    // points (a)bove lower region of test
  };

protected:
  // TODO: implement describe_core, as it is unused in the sequential version
  // void describe_core(const conflict_t &conflict);
  void asssumeIncomparableRegion(size_t wid, const YPoint &yp, Lit l);
  vec<Lit> explanation; // unsat explanation
  unique_ptr<waiting_list::WaitingListI> waiting_list;
  double runtime{};
  YPoint core_marker{};
  std::map<YPoint, Node> mem{};
  std::map<Lit, YPoint> slide_map{};
  std::vector<ParSlideDrillMO::SDWorkerState> worker_states;
};

class ParSlideDrillServerMO : public virtual ParallelMOServer,
                              public virtual ParSlideDrillMO {

public:
  ParSlideDrillServerMO(int verb = _VERBOSITY_MINIMAL_,
                        int weight = _WEIGHT_NONE_,
                        int strategy = _WEIGHT_NONE_,
                        int enc = _CARD_MTOTALIZER_, int pb = _PB_SWC_,
                        int pbobjf = _PB_GTE_, size_t nworkers = 2,
                        bool clausesharing = false,
                        int apmode = encoding::_ap_outvars_, float eps = 1,
                        int searchStrat = 3, bool ascend = false,
                        bool lower = false, int wl_type = 0)
      : ParallelMO(verb, weight, strategy, enc, pb, pbobjf, nworkers,
                   clausesharing),
        ParallelMOServer(verb, weight, strategy, enc, pb, pbobjf),
        ParSlideDrillMO(verb, weight, strategy, enc, pb, pbobjf, nworkers,
                        clausesharing, apmode, eps, searchStrat, ascend, lower,
                        wl_type) {}
};

} // namespace openwbo

#endif
