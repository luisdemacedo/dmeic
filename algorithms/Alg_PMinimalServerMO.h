#ifndef ALG_PMINIMALSERVERMO_H
#define ALG_PMINIMALSERVERMO_H
#include "Alg_PMinimalMO.h"
#include "Alg_ServerMO.h"

namespace openwbo{
  class PMinimalServerMO: public virtual PBtoCNFServerMO{
  public:
    PMinimalServerMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, int strategy = _WEIGHT_NONE_, 
		     int enc = _CARD_MTOTALIZER_, int pb = _PB_SWC_, 
		     int pbobjf = _PB_GTE_):
      PBtoCNF(verb, weight, strategy, enc, pb, pbobjf),
      PBtoCNFServerMO(verb,weight,strategy, enc,pb, pbobjf)
    {}
    void search_MO() override {};
    YPoint marker{};
    void build() override {};
    StatusCode searchAgain() override;
    bool searchPMinimalServerMO();
    void consolidateSolution() override {};
    void increment() override;
    bool not_done() override;
    void checkSols() override {return;};
    virtual void bootstrap(const Solution& sol) override {return;};

  protected:
    YPoint ul{};
  };
}

#endif
