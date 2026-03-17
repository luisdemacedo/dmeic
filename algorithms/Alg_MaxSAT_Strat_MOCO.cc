#include "Alg_MaxSAT_Strat_MOCO.h"
#include "Alg_UnsatSatMSU3MO.h"
#include "core/SolverTypes.h"
#include <cstdint>

void MaxSAT_Strat_MOCO::build(){
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
  for (int i = 0; i < getFormula()->nSoft(); i++){
    auto lit = getFormula()->getSoftClause(i).assumption_var;
    auto v = Glucose::var(lit);
    auto sign = Glucose::sign(lit);
    solver->setPolarity(v, !sign);
  }
}

void MaxSAT_Strat_MOCO::test_stratification_addsup(){
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

bool MaxSAT_Strat_MOCO::buildWorkFormula(){
  return UnsatSatMO::buildWorkFormula();
}

void MaxSAT_Strat_MOCO::search_MO(){
  build();
  if(!firstSolution()){
    printAnswer(_UNSATISFIABLE_);
    return;
  }
  // auto ub = upperBoundMCS();
  // auto of = getFormula()->resetObjFunction();
  // for(auto& entry: of)
  //   entry->ub((ub + entry->_const)/entry->_factor);
  // getFormula()->setObjFunction(std::move(of));


  buildWorkFormula();
  UnsatSatMO::searchUnsatSatMO();
  consolidateSolution();
  if(solution().size() > 0){
    answerType = _OPTIMUM_;
  }else{
    answerType = _UNSATISFIABLE_;
  }
  test_stratification_addsup();
  printAnswer(answerType);
}

void MaxSAT_Strat_MOCO::printAnswer(int type) {
  int64_t min = original.lb();
  int64_t max = original.ub();
  Model min_model {};
  Model max_model {};
  consolidateSolution();
  {
    auto end = solution().end();
    auto sol= solution().begin();
    if(sol != end){
      min_model=sol->second.first.model();
      min = original.evaluate(min_model, true);
      sol++;
      for(; sol!=end; sol++){
	auto cur = original.evaluate(sol->second.first.model(),true);
	if(cur < min){
	  min = cur;
	  min_model = sol->second.first.model();
	}
	if(cur > max){
	  max = cur;
	  max_model = sol->second.first.model();
	}
      }
    }
  }

  while(solution().size()){
    transferToEffSols();
    solution().pop();
  }
  if (verbosity > 0 && print)
    printStats();
  printf("c ---------- OUTPUT ---------------\n");

  if (type == _UNKNOWN_ && effsols.size() > 0)
    type = _SATISFIABLE_;
  else if (type == _UNKNOWN_ && model.size() > 0)
    type = _SATISFIABLE_;

  // store type in member variable
  searchStatus = (StatusCode)type;
  if(!print) return;

  switch (type) {
  case _SATISFIABLE_:
    printf("s SATISFIABLE\n");
    cout << "o "<< min << '\n';
    cout << "v ";
    min_model.print();
    cout << '\n';
    cout << "c max: "<< max << '\n';
    cout << "c max model: ";
    max_model.print();
    cout << '\n';
    clearLowerBoundSet(lbseti_expeps); //remove the last (incomplete) LBset
    // printEffSolutions(true);
    printMyStats();
    fflush(stdout);
    break;
  case _OPTIMUM_:
    printf("s OPTIMUM\n");
    cout << "o "<< min << '\n';
    cout << "v ";
    min_model.print();
    cout << '\n';
    cout << "c max: "<< max << '\n';
    cout << "c max model: ";
    max_model.print();
    cout << '\n';
    // printEffSolutions(true);
    printApproxRatio();
    printMyStats();
    fflush(stdout);
    break;
  case _UNSATISFIABLE_:
    printf("s UNSATISFIABLE\n");
    break;
  case _UNKNOWN_:
    printf("s UNKNOWN\n");
    break;
    
  case _MEMOUT_:
    printEffSolutions(false);
    printMyStats();
    printf("s MEMOUT\n");
    fflush(stdout);
    break;
  default:
    printf("c Error: Invalid answer type.\n");
  }
  exit(0); //AG - adicionei depois usar o setrlimit para ignorar o tempo de encoding (em Alg_PBtoCNF.cc)
}
int64_t MaxSAT_Strat_MOCO::upperBoundMCS(){
  int64_t ub = original.ub();
  for (int i = 0; i < maxsat_formula->nSoft(); i++)
    coreMapping[maxsat_formula->getSoftClause(i).assumption_var] = i;
  std::cout<<"c compute one MCS\n";
  // compute one MCS,
  solver->setConfBudget(100000);
  auto res =identifyDisjointCores();
  if(res == l_True){
    findNextCoreBasedMCS();
    if(_smallestModel.size())
      ub = original.evaluate(make_model(_smallestModel));
  };
  solver->budgetOff();
  //compute upper bound using original function?
  return ub;
}
  Model MaxSAT_Strat_MOCO::make_model(const vec<lbool>& m){
    vec<lbool> proj;
    for(auto n = maxsat_formula->nInitialVars() + maxsat_formula->nSoft(),
	  i = 0; i < n; i++)
      proj.push(m[i]);
    return Model{proj};
  }
