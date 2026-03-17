#ifndef ALG_STRATANALYSISMO_H
#define ALG_STRATANALYSISMO_H
#include "Alg_StratMO.h"
#include "../Converter.h"
class StratAnalysisMO: public virtual StratMO{
public:
  StratAnalysisMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, int strategy = _WEIGHT_NONE_, 
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
  StatusCode compute_approx() override {return StatusCode::_UNKNOWN_;};
  bool setup_approx() override {return true;};
  virtual void search_MO() override;
  virtual void updateStats() override;
private:
  PBObjFunction original;
  void test_stratification_addsup();
};
#endif
