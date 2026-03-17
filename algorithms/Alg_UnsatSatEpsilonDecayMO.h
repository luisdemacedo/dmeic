#ifndef ALG_UNSATSATEPSILONDECAYMO_H
#define ALG_UNSATSATEPSILONDECAYMO_H
#include "Alg_UnsatSatMO.h"
#include "Alg_ServerMO.h"
// this class implements another version of an unsat sat honer, with
// the added benefit of providing both lower and upper bound sets, and
// also providing a bound on the epsilon factor, i.e, the scaling
// factor that inverts the order relation between two comparable sets,
// moving the low one above the high one.  The epsilon factor also
// controls the progression of the lower bound set.
using conflict_t = vec<Lit>;
namespace epsilon{
class UnsatSatEpsilonDecayMO: public virtual UnsatSatMO, public virtual PBtoCNF{
protected:
  using conflict_t = vec<Lit>;
  constexpr static bool polarity = false; // because it is an unsat-sat solver
public:
  UnsatSatEpsilonDecayMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, int strategy = _WEIGHT_NONE_,
		    int enc = _CARD_MTOTALIZER_, int pb = _PB_SWC_, int pbobjf = _PB_GTE_, 
		    int apmode = encoding::_ap_outvars_, float eps = 1, int searchStrat=3, int conf_budget=-1,
		    bool core_ascend=false, int core_optim=1, bool core_block=true, int core_budget=-1, 
		    double geometric_ratio=1, int arithmetic_shift=1, bool block_below=true, bool drill=true):
    PBtoCNF(verb, weight, strategy, enc, pb, pbobjf),
    UnsatSatMO(verb, weight, strategy, enc, pb, pbobjf){
    _ascend = core_ascend;
    _core_optim = core_optim;
    _block = core_block;
    geo_ratio = geometric_ratio;
    arith_shift = arithmetic_shift;
    _block_below = block_below;
    conflict_core = core_budget;
    conflict_budget = conf_budget;
    conflict_limit = -1;
    _drill = drill;
  };

  ~UnsatSatEpsilonDecayMO(){}
  void search_MO() override;
  void printAnswer(int type) override;
  // find solutions below the current fence, until there is a epsilon
  // representative for every optimal solution.  Note that lbs is not
  // a lower bound set prior to running this function.
  void search_below();
  // move lbs to produce a new fence.  The new fence establishes the
  // epsilon factor for the next iteration.  The new fence is not a
  // lower bound set.
  YPoint extend_point(const YPoint& yp); 
  bool extend();
  // block region above yp.  Literal l is the deactivation variable:
  // if true, clause is deactivated.
  void optimize_lbs(conflict_t& conf);
  bool optimize_core_destructive(conflict_t& conf, set<Lit>& done);
  bool optimize_core_constructive(conflict_t& conf);
  bool block_region_below(const YPoint& yp);
  bool block_point_below(const YPoint& yp);
  bool drill(YPoint& yp);
protected:
  void sort_conflict(conflict_t& conflict, bool ascend);
  // lower bound set of dynamic instance.
  pareto::ThinSet<YPoint, false> lbs;
  // fence that will replace lbs after calling search_below.
  pareto::ThinSet<YPoint,false> fence;
  pareto::ThinSet<YPoint,false> corner;
  pareto::ThinSet<YPoint,false> e_sol;
  // lower bound set of original instance
  pareto::ThinGapSet<YPoint,false> e_gbs;
  std::multimap<YPoint, YPoint> e_gbs_t;
  std::map<Lit, YPoint> lit_to_point{};
  std::map<YPoint, Lit> point_to_lit{};
  double runtime{};
  conflict_t conf;
  bool _ascend;
  int _core_optim;
  bool _block;
  bool _block_below;
  double geo_ratio;
  int arith_shift;
  int conflict_core = -1;
  int conflict_budget = -1;
  bool _drill;
  std::map<YPoint, Lit> soft_blocks;
};

vec<Lit> less_than_clause(const conflict_t& conf, const conflict_t& rs);
void report_e_sols(std::multimap<YPoint, YPoint>& e_gbs_t);
}

#endif
