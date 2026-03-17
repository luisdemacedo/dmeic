#ifndef ALG_SLIDEMO_H
#define ALG_SLIDEMO_H
#include "./Alg_PBtoCNF.h"

namespace openwbo {
  class SlideMO: public virtual PBtoCNF{
    using conflict_t = vec<Lit>;
    constexpr static bool polarity = true; // because it is a sat-unsat solver;
  public:
    SlideMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, int strategy = _WEIGHT_NONE_, 
	    int enc = _CARD_MTOTALIZER_, int pb = _PB_SWC_, 
	    int pbobjf = _PB_GTE_):
      PBtoCNF(verb, weight, strategy, enc, pb, pbobjf){}

    ~SlideMO(){}

  public:
    void search_MO() override;
    bool searchSlideMO();
    bool slide();
    void asssumeIncomparableRegion(const YPoint& yp, Lit l);
    void describe_core(const conflict_t& conflict);
  protected:
    double runtime{};
    std::map<Lit, YPoint> slide_map{};
  };
}


#endif
