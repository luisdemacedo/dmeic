#ifndef ALG_HITTINGSETSSERVERMO_H
#define ALG_HITTINGSETSSERVERMO_H

#include "Alg_HittingSetsMO.h"


  class HittingSetsServerMO: public virtual HittingSetsMO, public virtual PBtoCNFServerMO{
  public:
    HittingSetsServerMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, int strategy = _WEIGHT_NONE_, 
			int enc = _CARD_MTOTALIZER_, int pb = _PB_SWC_, int pbobjf = _PB_GTE_):
      PBtoCNF(verb, weight, strategy, enc, pb, pbobjf),
      HittingSetsMO(verb,weight,strategy, enc,pb, pbobjf),
      PBtoCNFServerMO(verb,weight,strategy, enc,pb, pbobjf)
    {}
    void build() override;
    StatusCode searchAgain() override;
    void consolidateSolution() override;
    bool not_done() override;
    void increment() override;
    void checkSols() override;
    virtual void bootstrap(const Solution& sol) override;
    bool recycleLowerBoundSet() override;
  };

  class HittingSetsConflictServerMO: public virtual HittingSetsServerMO, public virtual PBtoCNFServerMO{
  public:
    HittingSetsConflictServerMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, int strategy = _WEIGHT_NONE_, 
			     int enc = _CARD_MTOTALIZER_, int pb = _PB_SWC_, int pbobjf = _PB_GTE_):
      PBtoCNF(verb, weight, strategy, enc, pb, pbobjf),
      HittingSetsMO(verb,weight,strategy, enc,pb, pbobjf),
      PBtoCNFServerMO(verb,weight,strategy, enc,pb, pbobjf),
      HittingSetsServerMO(verb,weight,strategy, enc,pb, pbobjf){}
    void increment() override;
  };




#endif
