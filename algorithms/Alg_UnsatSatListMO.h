#ifndef ALG_UNSATSATLISTMO_H
#define ALG_UNSATSATLISTMO_H
#include "Alg_UnsatSatMO.h"

class UnsatSatListMO: public virtual UnsatSatMO, public virtual PBtoCNFServerMO{
  constexpr static bool polarity = false; // because it is an unsat-sat solver

public:
  UnsatSatListMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, int strategy = _WEIGHT_NONE_,
		 int enc = _CARD_MTOTALIZER_, int pb = _PB_SWC_,
		 int pbobjf = _PB_GTE_, int apmode = encoding::_ap_outvars_, float eps = 1,
		 int searchStrat=3, bool ascend=false, bool lower=false, int wl_type=0):
    PBtoCNF(verb, weight, strategy, enc, pb, pbobjf),
    UnsatSatMO(verb, weight, strategy, enc, pb, pbobjf),
    PBtoCNFServerMO(verb, weight, strategy, enc, pb, pbobjf)
  {
    waiting_list = waiting_list::construct(wl_type, lower, ascend);
  }
  bool buildWorkFormula() override {return{};}
  Solver* getSolver() override {return solver;}
  StatusCode searchAgain() override {
    for(const auto& el: ls)
      waiting_list->insert(el);
    searchUnsatSatMO();
    return answerType;
  }
  // void checkSols() override;
  void increment() override;
  bool searchUnsatSatMO() override;
  bool rootedSearch(const YPoint& yp) override;
  // int blockStep(const YPoint& yp) override;
  // bool not_done() override;
  virtual void bootstrap(const Solution& sol) override {return;}
  bool soar();
  bool glide(const YPoint& yp);
  bool prune(const vec<Lit>& conflict, YPoint yp);
  bool check(const YPoint& yp);
  void build() override;
  void printAnswer(int type) override;
private:
  unique_ptr<waiting_list::WaitingListI> waiting_list;
  double runtime{};
  YPoint drill_marker{};
};



#endif
