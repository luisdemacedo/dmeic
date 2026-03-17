#ifndef ALG_MAXSAT_STRAT_MOCO_H
#define ALG_MAXSAT_STRAT_MOCO_H

#include "Alg_StratMO.h"
#include "../Converter.h"
#include "Alg_UnsatSatMSU3MO.h"
#include "core/SolverTypes.h"
#include <cstdint>
class MaxSAT_Strat_MOCO: public virtual UnsatSatMO, public virtual StratMO{
public:
  MaxSAT_Strat_MOCO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, int strategy = _WEIGHT_NONE_, 
		    int enc = _CARD_MTOTALIZER_, int pb = _PB_SWC_, 
		    int pbobjf = _PB_GTE_, int apmode = encoding::_ap_outvars_, float eps = 1, 
		    int searchStrat=3, int partition_parameter = 15, float redFact=-1): 
    PBtoCNF(verb, weight, strategy, enc, pb, pbobjf),
    StratMO(verb, weight, strategy, enc, pb, pbobjf, apmode, eps, searchStrat, partition_parameter, redFact),
    original{}
  {}


  void initializeOptimizer(Solver*, MaxSATFormula*) override {};
  bool incorporate_approx() override {return true;};
  void build() override;
  bool buildWorkFormula() override;
  void printAnswer(int type) override;
  StatusCode compute_approx() override {return StatusCode::_UNKNOWN_;};
  bool setup_approx() override {return true;};
  virtual void search_MO() override;
  int64_t upperBoundMCS();
  Model make_model(const vec<lbool>&) override;
private:
  PBObjFunction original;
  void test_stratification_addsup();
};
#endif
