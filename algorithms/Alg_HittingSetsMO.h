#ifndef ALG_HITTINGSETSMO_H
#define ALG_HITTINGSETSMO_H

#include "Alg_MasterMO.h"
#include "../Encoder.h"

#include "../MaxSAT.h"
#include "./Alg_PBtoCNF.h"
#include "./Alg_ServerMO.h"
#include "./Alg_UnsatSatMO.h"
#include "./Alg_UnsatSatIncHSMO.h"
#include "./Alg_UnsatSatMSU3MO.h"
#include "./Alg_UnsatSatStratMSU3MO.h"
#include "utils/System.h"
#include <utility>
#include <map>
#include <set>

#define MAXDIM 50

namespace openwbo {
    
  
  class HittingSetsMO : public virtual PBtoCNFMasterMO {
    class diagnosis: public std::pair<int, std::vector<Lit>>{
    public:
      int& id(){return first;}
      std::vector<Lit>& clause(){return second;}
};
  public:
    HittingSetsMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, int strategy = _WEIGHT_NONE_, 
	       int enc = _CARD_MTOTALIZER_, int pb = _PB_SWC_, 
		  int pbobjf = _PB_GTE_) : 
      PBtoCNF(verb, weight, strategy, enc, pb, pbobjf)
{
  optim_sliced = new UnsatSatIncHSMO(verb, weight, strategy, enc, pb, pbobjf);
  optim = optim_sliced;
}
    
    ~HittingSetsMO(){
      if (optim != NULL)
        delete optim;
      optim = NULL;
    }
    void vectorVec(const std::vector<Lit>& vector, vec<Lit>& vec);
    void genLowerBoundSet();
    bool buildWorkFormula() override;
    void incrementFormula();
    bool absorb(Solution::OneSolution& osol, int bvar);
    bool diagnose(Solution::OneSolution& osol, vec<Lit>&);
    bool virtual recycleLowerBoundSet();
    void initializeOptimizer(Solver* solv, MaxSATFormula* mxf) override;
    void consolidateSolution() override;
    bool setup_approx() override;
    bool incorporate_approx()override;
    void build() override;
    

  protected:
    std::vector<diagnosis> diagnoses{};
    UnsatSatIncHSMO* optim_sliced;
  };
}

#endif
