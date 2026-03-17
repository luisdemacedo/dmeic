#ifndef ALG_DRILL_H
#define ALG_DRILL_H
#include <map>
#include "utility"

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include "../Encoder.h"

#include "../MaxSAT.h"
#include "./Alg_PBtoCNF.h"
#include "utils/System.h"
#include <utility>
#include "./Alg_ServerMO.h"
#include "../waiting_list/waiting_list.h"

namespace openwbo {
  using conflict_t = vec<Lit>;
  class DrillMO : public virtual PBtoCNF, public virtual Bounded{
    constexpr static bool polarity = true; // because it is a sat-unsat solver;
    class Node: public YPoint {
    public:
      Node(){}
      Node(YPoint& ypo): YPoint{ypo}{}
      vector<Lit> deps{};
    };

  public:
    DrillMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, int strategy = _WEIGHT_NONE_, 
		 int enc = _CARD_MTOTALIZER_, int pb = _PB_SWC_, 
		 int pbobjf = _PB_GTE_, int apmode = encoding::_ap_outvars_, float eps = 1, 
		 int searchStrat=3, bool ascend=false, bool lower=false, int wl_type=0) : 
		   PBtoCNF(verb, weight, strategy, enc, pb, pbobjf) {
      waiting_list = waiting_list::construct(wl_type, lower, ascend);
    }
    
    ~DrillMO(){}
    
    bool searchBoundHonerMO();
    void search_MO() override;
    bool slide();
    bool drill();
    // compile test ran by check
    bool prune(const vec<Lit>& conflict, YPoint yp);
    bool check(YPoint yp);
  protected:
    void describe_core(const conflict_t& conflict);
    void asssumeIncomparableRegion(const YPoint& yp, Lit l);
    vec<Lit> explanation; 	// unsat explanation
    unique_ptr<waiting_list::WaitingListI> waiting_list;
    double runtime{};
    YPoint drill_marker{};
    YPoint core_marker{};
    // results of slide, and corresponding blocking variables
    std::map<YPoint, Node> mem{};
    std::map<Lit, YPoint> slide_map{};
    // points (a)bove lower region of test
  };

  class DrillServerMO: public virtual PBtoCNFServerMO, public virtual DrillMO{
  public:
    DrillServerMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, int strategy = _WEIGHT_NONE_, 
			    int enc = _CARD_MTOTALIZER_, int pb = _PB_SWC_, 
			    int pbobjf = _PB_GTE_, int apmode = encoding::_ap_outvars_, float eps = 1, 
		       int searchStrat=3, bool ascend=false, bool lower=false, int wl_type=0):
      PBtoCNF(verb, weight, strategy, enc, pb, pbobjf),
      PBtoCNFServerMO(verb,weight,strategy, enc,pb, pbobjf),
      DrillMO(verb,weight,strategy, enc,pb, pbobjf,apmode,eps, searchStrat, ascend, lower, wl_type)
    {};
  
    void build() override {
      waiting_list->insert(pareto::max);
    };
    void printAnswer(int type) override;
    StatusCode searchAgain() override;
    void consolidateSolution() override {};
    void increment() override;
    bool not_done() override;
    void checkSols() override {return;};
    virtual void bootstrap(const Solution& sol) override {return;};
  }
    ;
}

#endif
