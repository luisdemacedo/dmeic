#include "Alg_StratAnalysisMO.h"
#include "Alg_PBtoCNF.h"
#include <memory>

void StratAnalysisMO::build(){
  init();
  nbMCS = 0;
  answerType = _UNKNOWN_;
  mf = make_shared<MOCOFormula>(converter::MaxSATtoMOCO{}.convert(*getMaxSATFormula()));
  original = PBObjFunction{*mf->getObjFunction(0)};
  std::cout<<"c maxsat objective:\n";
  original.my_print(getFormula()->maxsat_formula()->getIndexToName(), true, 1000);
  mf = make_shared<MOCOFormula>(converter::MOCOtoBrokenMOCO{}.convert(*mf, part_par()));
  for(int i = 0; i < getFormula()->nObjFunctions(); i++)
    objRootLits.push_back(std::make_shared<rootLits::RootLits>(rootLits::RootLits{}));
solver = buildSolverMO();
}

void StratAnalysisMO::test_stratification_addsup(){
  auto sum = PBObjFunction{*mf->getObjFunction(0)};
  for(int i = 1, n = mf->nObjFunctions(); i < n; i++)
    sum = PBObjFunction{*openwbo::add(&sum, mf->getObjFunction(i))};
  if(sum == original)
    cout<<"sucess: functions are the same\n";
  else{
    cout << "things are different...\n";
    cout << "sum is\n";
    sum.my_print(getFormula()->maxsat_formula()->getIndexToName(), true, 1000);
    cout << "original is\n";
    original.my_print(getFormula()->maxsat_formula()->getIndexToName(), true, 1000);
  }
  assert(sum == original);

}

void StratAnalysisMO::search_MO(){
  build();
  // buildWorkFormula();
  test_stratification_addsup();
  printAnswer(_OPTIMUM_);
}

void StratAnalysisMO::updateStats(){
}
