#ifndef ALG_CONFLICTSHUNTMO_H
#define ALG_CONFLICTSHUNTMO_H
#include "./Alg_SlideDrillMO.h"
#include "Alg_MasterMO.h"
#include "Alg_ServerMO.h"
#include "Alg_UnsatSatListMO.h"
#include "Alg_UnsatSatHonerMO.h"
#include "./Alg_SlideDrillMO.h"
#include "./Alg_PMinimalServerMO.h"

#include <memory>
namespace debug{
  class ConflictShuntMO_debugger;
}

class ConflictShuntMO: public PBtoCNFMasterMO{
  
public:
  friend class debug::ConflictShuntMO_debugger;    
  ConflictShuntMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, 
		  int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_, 
		  int pb = _PB_SWC_, int pbobjf = _PB_GTE_, 
		  int apmode = encoding::_ap_outvars_, float eps = 1, 
		  int searchStrat=3, int conf_budget=INT32_MAX, int wl_type=0):
    PBtoCNF(verb, weight, strategy, enc, 
	pb, pbobjf) {
    setConflictLimit(conf_budget);
  }
  void printAnswer(int type) override;
  void search_MO() override;
  bool buildWorkFormula() override;
  bool setup_approx() override;
  bool incorporate_approx() override;
  void build() override;
  void consolidateSolution() override;
  void initializeOptimizer(Solver*, MaxSATFormula*) override;
  StatusCode searchConflictShuntMO();
protected:
  shared_ptr<PBtoCNFServerMO> upper{};
  shared_ptr<PBtoCNFServerMO> lower{};
};


class SlideDrillShuntMO:  public ConflictShuntMO {
public:

  SlideDrillShuntMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, 
		       int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_, 
		       int pb = _PB_SWC_, int pbobjf = _PB_GTE_, 
		       int apmode = encoding::_ap_outvars_, float eps = 1, 
		    int searchStrat=3, int conf_budget=INT32_MAX, bool ascend=false, bool lower=false, int wl_type=0):
			 PBtoCNF(verb, weight, strategy, enc, 
				 pb, pbobjf),
			 ConflictShuntMO(verb, weight, strategy, enc, 
					 pb, pbobjf, apmode, eps, searchStrat, conf_budget) {
    upper = make_shared<SlideDrillServerMO>(verb, weight, strategy, enc, 
					    pb, pbobjf, apmode, eps, 
					    searchStrat, ascend, lower, wl_type);
    
    optim = upper.get();
  }


};


class UnsatSatShuntMO:  public ConflictShuntMO {
public:

  UnsatSatShuntMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, 
		  int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_, 
		  int pb = _PB_SWC_, int pbobjf = _PB_GTE_, 
		  int apmode = encoding::_ap_outvars_, float eps = 1, 
		  int searchStrat=3, int conf_budget=INT32_MAX, bool ascend=false, bool lower_b=false, int wl_type=0):
		    PBtoCNF(verb, weight, strategy, enc, 
			    pb, pbobjf),
		    ConflictShuntMO(verb, weight, strategy, enc, 
				    pb, pbobjf, apmode, eps, searchStrat, conf_budget) {
    lower = make_shared<UnsatSatListMO>(verb, weight, strategy, enc,
					pb, pbobjf, apmode, eps, 
					searchStrat, ascend, lower_b, wl_type);

    optim = lower.get();
  }


};


class SlideUnsatSatHonerShuntMO:  public ConflictShuntMO {
public:

  SlideUnsatSatHonerShuntMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, 
			    int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_, 
			    int pb = _PB_SWC_, int pbobjf = _PB_GTE_, 
			    int apmode = encoding::_ap_outvars_, float eps = 1, 
			    int searchStrat=3, int conf_budget=INT32_MAX, bool ascend=false, 
			    bool lowerb=false, int wl_type=0, bool core_ascend=false,
			    bool core_optim=true,
			    bool core_block=true):
    PBtoCNF(verb, weight, strategy, enc, pb, pbobjf),
    ConflictShuntMO(verb, weight, strategy, enc, 
		    pb, pbobjf, apmode, eps, searchStrat, conf_budget) {
    upper = make_shared<SlideDrillServerMO>(verb, weight, strategy, enc, 
					    pb, pbobjf, apmode, eps, 
					    searchStrat, ascend, lowerb, wl_type);
    
    lower = make_shared<UnsatSatHonerServerMO>(verb, weight, strategy, enc,
					       pb, pbobjf, apmode, eps, 
					       searchStrat, core_ascend, core_optim, core_block);
    optim = lower.get();
  }
};






class SlideDrillUnsatSatShuntMO:  public ConflictShuntMO, public virtual Bounded {
public:

  SlideDrillUnsatSatShuntMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, 
		       int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_, 
		       int pb = _PB_SWC_, int pbobjf = _PB_GTE_, 
		       int apmode = encoding::_ap_outvars_, float eps = 1, 
			    int searchStrat=3, int conf_budget=INT32_MAX, bool ascend=false, bool lowerb=false, int wl_type=0):
			 PBtoCNF(verb, weight, strategy, enc, pb, pbobjf),
			 ConflictShuntMO(verb, weight, strategy, enc, 
					 pb, pbobjf, apmode, eps, searchStrat, conf_budget) {
    upper = make_shared<SlideDrillServerMO>(verb, weight, strategy, enc, 
					    pb, pbobjf, apmode, eps, 
					    searchStrat, ascend, lowerb, wl_type);
    
    lower = make_shared<UnsatSatListMO>(verb, weight, strategy, enc,
					pb, pbobjf, apmode, eps, 
					searchStrat, true, false, 2);
    optim = lower.get();
  }

  bool setup_approx() override{
    lower_set(std::move(optim->ls));
    upper_set(std::move(optim->us));
    if(lower.get() && upper.get()){
      optim = (optim == upper.get())? lower.get(): upper.get();
    }
    optim->lower_set(std::move(ls));
    optim->upper_set(std::move(us));
    optim->increment();
    return optim->not_done();
  }
};


class UnsatSatListPMinimalMO: public ConflictShuntMO{
  
public:
  UnsatSatListPMinimalMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, 
		  int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_, 
		  int pb = _PB_SWC_, int pbobjf = _PB_GTE_, 
		  int apmode = encoding::_ap_outvars_, float eps = 1, 
		  int searchStrat=3, int conf_budget=INT32_MAX, int wl_type=0):
    PBtoCNF(verb, weight, strategy, enc, pb, pbobjf) {
    setConflictLimit(conf_budget);
    upper = make_shared<PMinimalServerMO>(verb, weight, strategy, enc, 
					  pb, pbobjf);
    
    lower = make_shared<UnsatSatListMO>(verb, weight, strategy, enc,
					pb, pbobjf, apmode, eps, 
					searchStrat, false, false, wl_type);
    optim = upper.get();
  }
};


class UnsatSatPMinimalMO: public ConflictShuntMO{
  
public:
  friend class debug::ConflictShuntMO_debugger;    
  UnsatSatPMinimalMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, 
		  int strategy = _WEIGHT_NONE_, int enc = _CARD_MTOTALIZER_, 
		  int pb = _PB_SWC_, int pbobjf = _PB_GTE_, 
		  int apmode = encoding::_ap_outvars_, float eps = 1, 
		  int searchStrat=3, int conf_budget=INT32_MAX):
    PBtoCNF(verb, weight, strategy, enc, pb, pbobjf) {
    setConflictLimit(conf_budget);
    upper = make_shared<PMinimalServerMO>(verb, weight, strategy, enc, pb, pbobjf);
    
    lower = make_shared<UnsatSatIncMO>(verb, weight, strategy, enc, pb, pbobjf);
    optim = upper.get();
  }
};


#endif

