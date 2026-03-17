#ifndef UNSATSATBUDGETMO
#define UNSATSATBUDGETMO
// #define PARTIAL
#include "core/SolverTypes.h"
#include <memory>
#ifndef PARTIAL

#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif

#include "../Encoder.h"
#include "../Pareto.h"
#include "../MaxSAT.h"
#include "./Alg_ServerMO.h"
#include "utils/System.h"
#include <utility>
#include "../partition.h"
#include <algorithm>    // std::max
#include <random>
#include "../waiting_list/waiting_list.h"

#define MAXDIM 50

namespace openwbo {
  using namespace partition;
  
  class UnsatSatBudgetMO : public virtual PBtoCNF {
    
  public:
    UnsatSatBudgetMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, int strategy = _WEIGHT_NONE_, 
	       int enc = _CARD_MTOTALIZER_, int pb = _PB_SWC_, 
		     int pbobjf = _PB_GTE_, int core_budget=-1, int conf_budget=-1, bool core_block=true) : 
      PBtoCNF(verb, weight, strategy, enc, pb, pbobjf){
      conflict_core = core_budget;
      _block = core_block;
      // setup budget
      toggleConflictBudget(conf_budget);
      // deactivate it by default
      toggleConflictBudget();
    }
    
    std::vector<MyPartition> generate();
    MyPartition mix(std::vector<MyPartition>);

    bool buildWorkFormula() override;
    StatusCode searchAgain();
    virtual bool searchUnsatSatMO();
    void search_MO() override;
    bool extendUL(uint64_t * upperObjv, uint64_t * upperObix);
    virtual bool extendUL(YPoint& ul);
    const std::set<Lit>& blocked_vars(){return blockedVars;};
  protected:
    vec<Lit> explanation{}; 	// unsat explanation
    virtual bool rootedSearch(const YPoint& yp);
    std::set<Lit> blockedVars{};    
    YPoint marker{};
    bool optimize_core_destructive(vec<Lit>& conflict,  set<Lit>& done);
    int conflict_core = -1;
    bool _block=true;
  };
}

#endif
#undef PARTIAL
#endif
