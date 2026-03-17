#ifndef ALG_UNSATSATHONERMO_H
#define ALG_UNSATSATHONERMO_H
#include "Alg_UnsatSatMO.h"
#include "Alg_ServerMO.h"

class UnsatSatHonerMO: public virtual UnsatSatMO, public virtual PBtoCNF{
protected:
  using conflict_t = vec<Lit>;
  constexpr static bool polarity = false; // because it is an unsat-sat solver
public:
  UnsatSatHonerMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, int strategy = _WEIGHT_NONE_,
		  int enc = _CARD_MTOTALIZER_, int pb = _PB_SWC_, int pbobjf = _PB_GTE_, 
		  int apmode = encoding::_ap_outvars_, float eps = 1, int searchStrat=3, 
		  bool core_ascend=false, bool core_constructive=true, bool core_block=true):
    PBtoCNF(verb, weight, strategy, enc, pb, pbobjf),
    UnsatSatMO(verb, weight, strategy, enc, pb, pbobjf){
    _ascend = core_ascend;
    _constructive = core_constructive;
    _block = core_block;
  };

  ~UnsatSatHonerMO(){}
  void search_MO() override;
  void search_below();
  bool extend();
  void blockAbove(const YPoint& yp, Lit l);
  void optimize_lbs(conflict_t& conf);
  bool optimize_core_destructive(conflict_t& conf);
  bool optimize_core_constructive(conflict_t& conf);
  void less_than_clause(const conflict_t& conf, const conflict_t& rs);
protected:
  void sort_conflict(conflict_t& conflict, bool ascend);
  pareto::ThinSet<YPoint> lbs;
  std::map<Lit, YPoint> tmp_block_map{};
  double runtime{};
  conflict_t conf;
  bool _ascend;
  bool _constructive;
  bool _block;

};

class UnsatSatHonerServerMO: public virtual PBtoCNFServerMO, public virtual UnsatSatHonerMO{
public:
  UnsatSatHonerServerMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, 
			int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_, 
			int pb = _PB_SWC_, int pbobjf = _PB_GTE_, 
			int apmode = encoding::_ap_outvars_, float eps = 1, int searchStrat=3, 
			bool core_ascend=false, bool core_constructive=true, bool core_block=true):
    PBtoCNF(verb, weight, strategy, enc, pb, pbobjf),
    PBtoCNFServerMO(verb,weight,strategy, enc,pb, pbobjf),
    UnsatSatMO(verb, weight, strategy, enc, pb, pbobjf),
    UnsatSatHonerMO(verb,weight,strategy, enc,pb, pbobjf,apmode,eps, searchStrat, core_ascend, 
		    core_constructive, core_block)
  {
};
  
  void build() override {
  };
  void printAnswer(int type) override;
  StatusCode searchAgain() override;
  void consolidateSolution() override {};
  void increment() override;
  bool not_done() override;
  void checkSols() override {return;};
  virtual void bootstrap(const Solution& sol) override {return;};
};
#endif
