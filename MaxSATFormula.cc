/*!
 * \author Ruben Martins - ruben@sat.inesc-id.pt
 *
 * @section LICENSE
 *
 * Open-WBO, Copyright (c) 2013-2015, Ruben Martins, Vasco Manquinho, Ines Lynce
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <iostream>

#include "MaxSATFormula.h"

using namespace openwbo;

MaxSATFormula *MaxSATFormula::copyMaxSATFormula() {
  assert(format == _FORMAT_MAXSAT_);

  MaxSATFormula *copymx = new MaxSATFormula();
  copymx->setInitialVars(nVars());

  for (int i = 0; i < nVars(); i++)
    copymx->newVar();

  for (int i = 0; i < nSoft(); i++)
    copymx->addSoftClause(getSoftClause(i).weight, getSoftClause(i).clause);

  for (int i = 0; i < nHard(); i++)
    copymx->addHardClause(getHardClause(i).clause);

  copymx->setProblemType(getProblemType());
  copymx->updateSumWeights(getSumWeights());
  copymx->setMaximumWeight(getMaximumWeight());
  copymx->setHardWeight(getHardWeight());
  copymx->setFixedVars(fixed_vars());

  return copymx;
}

MaxSATFormula *MaxSATFormula::copyPBFormula() {
  assert(format == _FORMAT_PB_);

  setFormat(_FORMAT_MAXSAT_);
  MaxSATFormula *copypb = copyMaxSATFormula();
  setFormat(_FORMAT_PB_);

  for (int i = 0; i < nPB(); i++)
    copypb->addPBConstraint(getPBConstraint(i));

  for (int i = 0; i < nCard(); i++)
    copypb->addCardinalityConstraint(getCardinalityConstraint(i));

  for (int i = 0; i < nObjFunctions(); i++)
    copypb->addObjFunction(getObjFunction(i));

  copypb->setFormat(_FORMAT_PB_);
  copypb->setProblemType(getProblemType());

  copypb->setIndexToName(getIndexToName());
  copypb->setNameToIndex(getNameToIndex());

  memcpy(copypb->bounds, bounds, sizeof(int64_t) * 4 * MAXDIM);

  strncpy(copypb->ifname, ifname, max::name);
  copypb->ifname[max::name - 1] = '\0'; // ensure null-termination

  return copypb;
}

// Adds a new hard clause to the hard clause database.
void MaxSATFormula::addHardClause(vec<Lit> &lits) {
  hard_clauses.push();
  vec<Lit> copy_lits;
  lits.copyTo(copy_lits);
  new (&hard_clauses[hard_clauses.size() - 1]) Hard(copy_lits);
  n_hard++;
}

// Adds a new soft clause to the hard clause database.
void MaxSATFormula::addSoftClause(uint64_t weight, vec<Lit> &lits) {
  soft_clauses.push();
  vec<Lit> vars;
  Lit assump = lit_Undef;
  vec<Lit> copy_lits;
  lits.copyTo(copy_lits);

  new (&soft_clauses[soft_clauses.size() - 1])
      Soft(copy_lits, weight, assump, vars);
  n_soft++;
}

// Adds a new soft clause to the hard clause database with predefined relaxation
// variables.
void MaxSATFormula::addSoftClause(uint64_t weight, vec<Lit> &lits,
                                  vec<Lit> &vars) {
  soft_clauses.push();
  Lit assump = lit_Undef;
  vec<Lit> copy_lits;
  lits.copyTo(copy_lits);

  new (&soft_clauses[soft_clauses.size() - 1])
      Soft(copy_lits, weight, assump, vars);
  n_soft++;
}

int MaxSATFormula::nInitialVars() {
  return n_initial_vars;
} // Returns the number of variables in the working MaxSAT formula.

void MaxSATFormula::setInitialVars(int vars) { n_initial_vars = vars; }

int MaxSATFormula::nVars() {
  return n_vars;
} // Returns the number of variables in the working MaxSAT formula.

int MaxSATFormula::nSoft() {
  return n_soft;
} // Returns the number of soft clauses in the working MaxSAT formula.

int MaxSATFormula::nHard() {
  return n_hard;
} // Returns the number of hard clauses in the working MaxSAT formula.

void MaxSATFormula::newVar(int v) {
  if (v == -1)
    n_vars++;
  else if (v > n_vars)
    n_vars = v;
} // Increases the number of variables in the working MaxSAT formula.

// Makes a new literal to be used in the working MaxSAT formula.
Lit MaxSATFormula::newLiteral(bool sign) {
  Lit p = mkLit(nVars(), sign);
  newVar();
  return p;
}

void MaxSATFormula::setProblemType(int type) {
  problem_type = type;
} // Sets the problem type.

int MaxSATFormula::getProblemType() {
  return problem_type; // Return the problem type.
}

// 'ubCost' is initialized to the sum of weights of the soft clauses.
void MaxSATFormula::updateSumWeights(uint64_t weight) {
  if (weight != hard_weight)
    sum_soft_weight += weight;
}

// The initial 'currentWeight' corresponds to the maximum weight of the soft
// clauses.
void MaxSATFormula::setMaximumWeight(uint64_t weight) {
  if (weight > max_soft_weight && weight != hard_weight)
    max_soft_weight = weight;
}

uint64_t MaxSATFormula::getMaximumWeight() { return max_soft_weight; }

void MaxSATFormula::setHardWeight(uint64_t weight) {
  hard_weight = weight;
} // Sets the weight of hard clauses.

Soft &MaxSATFormula::getSoftClause(int pos) {
  assert(pos < nSoft());
  return soft_clauses[pos];
}

Hard &MaxSATFormula::getHardClause(int pos) {
  assert(pos < nHard());
  return hard_clauses[pos];
}

void MaxSATFormula::addCardinalityConstraint(Card *card) {
  cardinality_constraints.push(card);
}

void MaxSATFormula::addPBConstraint(PB *p) {

  // Add constraint to formula data structure.
  if (p->isClause()) {
    addHardClause(p->_lits);
  } else if (p->isCardinality()) {
    if (!p->_sign) {
      p->changeSign();
    }
    cardinality_constraints.push(new Card(p->_lits, p->_rhs));

  } else {
    if (!p->_sign) {
      p->changeSign();
    }

    pb_constraints.push(new PB(p->_lits, p->_coeffs, p->_rhs, p->_sign));
  }
}

int MaxSATFormula::newVarName(char *varName) {
  int id = varID(varName);
  if (id == var_Undef) {
    id = nVars();
    newVar();
    std::string s(varName);
    std::pair<std::string, int> nv(s, id);
    std::pair<int, std::string> ni(id, s);
    _nameToIndex.insert(nv);
    _indexToName.insert(ni);
  }
  return id;
}

int MaxSATFormula::varID(char *varName) {
  std::string s(varName);

  nameMap::const_iterator iter = _nameToIndex.find(s);
  if (iter != _nameToIndex.end()) {
    return iter->second;
  }
  return var_Undef;
}

void MaxSATFormula::convertPBtoMaxSAT() {
  // void MaxSATFormula::convertPBtoMaxSAT(Solver * S = NULL) { //AG

  //   printf("MaxSATFormula::convertPBtoMaxSAT n_objf: %d\n", n_objf);
  assert(objective_functions != NULL);
  vec<Lit> unit_soft(1);

  if (n_objf == 1) {
    // Convert objective function to soft clauses
    for (int i = 0; i < objective_functions[0]->_lits.size(); i++) {
      assert(objective_functions[0]->_coeffs[i] > 0);
      unit_soft[0] = ~objective_functions[0]->_lits[i];
      addSoftClause(objective_functions[0]->_coeffs[i], unit_soft);

      // Updates the maximum weight of soft clauses.
      setMaximumWeight(objective_functions[0]->_coeffs[i]);
      // Updates the sum of the weights of soft clauses.
      updateSumWeights(objective_functions[0]->_coeffs[i]);
    }
  } else { // AG - n_objf > 1 (Multiobjective case)
    // TODO: converter fobj para GTE
    //  objective_function->_lits[i] ou ~objective_function->_lits[i]?
    for (int j = 0; j < n_objf; j++) {
      int totalw = 0;
      for (int i = 0; i < objective_functions[j]->_coeffs.size(); i++) {
        totalw += objective_functions[j]->_coeffs[i];
      };

      // TODO: Fazer versao standalone to GTE (recebe pbc e devolve conjunto de
      // clausulas e literais
      //              na raiz)
      //          encode(this, objective_functions[j]->_lits,
      //          objective_functions[j]->_coeffs, totalw);
    }
    //      obter os literais do no raiz, R

    //      usar o order encoding nos literais de R (adicionar hard clauses)

    //      adicionar os literais em R como soft clauses
  }

  if (getMaximumWeight() == 1)
    setProblemType(_UNWEIGHTED_);
  else
    setProblemType(_WEIGHTED_);

  // AG
  //   printf("MaxSATFormula::convertPBtoMaxSAT -- formula after conversion\n");
  //   my_print();
}

void Hard::my_print(indexMap indexToName) {
  printf("c\t");
  for (int i = 0; i < clause.size(); i++)
    //         printf(" %s%d", ((sign(clause[i])) ? "~" : ""), var(clause[i]));
    if (indexToName.find(var(clause[i])) != indexToName.end())
      printf(" %s%s", ((sign(clause[i])) ? "~" : ""),
             indexToName.at(var(clause[i])).c_str());
    else
      printf(" %s%s", ((sign(clause[i])) ? "~" : ""), "X");
  printf("\n");
}

void Soft::my_print(indexMap indexToName) {
  printf("c\t");
  for (int i = 0; i < clause.size(); i++)
    //         printf(" %s%d", ((sign(clause[i])) ? "~" : ""), var(clause[i]));

    if (indexToName.find(var(clause[i])) != indexToName.end())
      printf(" %lu %s%s", weight, ((sign(clause[i])) ? "~" : ""),
             indexToName.at(var(clause[i])).c_str());
    else
      printf(" %s%d", ((sign(clause[i])) ? "~" : ""), var(clause[i]));
  printf(" [");
  for (int i = 0; i < relaxation_vars.size(); i++)
    printf(" %s%d", ((sign(relaxation_vars[i])) ? "~" : ""),
           var(relaxation_vars[i]));
  printf("]");
  printf(" A(%d)\n", var(assumption_var));
}

void MaxSATFormula::my_print() {
  printf("c -----------------------------------\n");
  printf("c --------- index to var ------------\n");
  //     int k;
  //     string v;
  /*
  for (auto const& pair: _indexToName) {
      std::cout << "c\t{" << pair.first+1 << ": " << pair.second << "}\n";
  }*/

  //     int maxsize = 20;
  int maxcs = 10; // 100
  printf("c ------- Objective Func (%d) -----------------\n", n_objf);
  for (int di = 0; di < n_objf; di++)
    objective_functions[di]->my_print(_indexToName);

  printf("c ------- PB Constraints (%d) ------------\n", pb_constraints.size());
  for (int i = 0; i < pb_constraints.size(); i++) {
    pb_constraints[i]->my_print(_indexToName);
  }

  printf("c ------- Cardinality Constr (%d) --------\n",
         cardinality_constraints.size());
  if (cardinality_constraints.size() < maxcs) {
    for (int i = 0; i < cardinality_constraints.size(); i++) {
      cardinality_constraints[i]->my_print(_indexToName);
    }
  }

  printf("c ---------------------------------------\n");
  printf("c ------- Hard clauses (%d) --------------\n", hard_clauses.size());
  if (hard_clauses.size() < maxcs) {
    for (int i = 0; i < hard_clauses.size(); i++) {
      hard_clauses[i].my_print(_indexToName);
    }
  }
  printf("c ------- Soft clauses (%d) --------------\n", soft_clauses.size());
  if (soft_clauses.size() < maxcs) {
    for (int i = 0; i < soft_clauses.size(); i++) {
      soft_clauses[i].my_print(_indexToName);
    }
  }

  printf("c ---------------------------------------\n");
}

void MaxSATFormula::sync_first(Glucose::Solver *s) {
  for (int i = 0; i < nInitialVars(); i++)
    if (s->value(i) != l_Undef)
      fv.insert(mkLit(i, true));
}
