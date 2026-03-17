#ifndef ALG_PMINIMALMO_H
#define ALG_PMINIMALMO_H

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
#include <map>
#include <set>

#define MAXDIM 50

namespace openwbo {
    
  
  class PMinimalMO : public PBtoCNF {
    
  public:
    PMinimalMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, int strategy = _WEIGHT_NONE_, 
	       int enc = _CARD_MTOTALIZER_, int pb = _PB_SWC_, 
	       int pbobjf = _PB_GTE_, int apmode = encoding::_ap_outvars_, float eps = 1, 
	       int searchStrat=3, float redFact=-1) : 
      PBtoCNF(verb, weight, strategy, enc, pb, pbobjf){
    }
    
    ~PMinimalMO(){}
    
    bool searchPMinimalMO();
    void search_MO() override;
  protected:
    vec<Lit> explanation; 	// unsat explanation


  };
}

#endif
