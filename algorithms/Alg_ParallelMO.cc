#include "Alg_ParallelMO.h"

using namespace openwbo;
using NSPACE::toLit;

void ParallelMO::init() { // Copied from PBtoCNF
  vec<int> vars;
  vars.growTo(getFormula()->nVars(), 0);

  if (!_useAllVars) {
    for (int i = 0; i < getFormula()->nSoft(); i++) {
      for (int j = 0; j < getFormula()->getSoftClause(i).clause.size(); j++) {
        int v = var(getFormula()->getSoftClause(i).clause[j]);
        assert(v < vars.size());
        if (vars[v] == 0) {
          _soft_variables.push(v);
          vars[v] = 1;
        }
      }
    }
  } else {
    for (int i = 0; i < getFormula()->nVars(); i++) {
      _soft_variables.push(i);
    }
  }
  _assigned_true.growTo(getFormula()->nInitialVars(), 0);
  _varScore.growTo(getFormula()->nInitialVars(), 0);

  _maxWeight = 0;
  for (int i = 0; i < getFormula()->nSoft(); i++) {
    Lit l = getFormula()->newLiteral();
    getFormula()->getSoftClause(i).relaxation_vars.push(l);
    getFormula()->getSoftClause(i).assumption_var =
        getFormula()->getSoftClause(i).relaxation_vars[0]; // Assumption
                                                           // Var is
                                                           // relaxation
                                                           // var

    _maxWeight += getFormula()->getSoftClause(i).weight;
  }
  printf("c Max. Weight: %ld\n", _maxWeight);
  // build the objRootLits vector
  for (int i = objRootLits.size(); i < getFormula()->nObjFunctions(); i++)
    objRootLits.push_back(
        std::make_shared<rootLits::RootLits>(rootLits::RootLits{}));
}

bool ParallelMO::updateMOFormulationIfSAT() {
  printf("c [updateMOFormulationIfSAT]\n");
  //     solver->my_print();

  // if (!firstSolution())
  //   return false;
  // updateMOFormulation();
  return true;
}

bool ParallelMO::updateMOFormulation() {

  return true; // TODO: implement the update of the MO formulation after finding
               // a solution
}
