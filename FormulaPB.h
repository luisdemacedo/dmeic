/*!
 * \author Vasco Manquinho - vmm@sat.inesc-id.pt
 *
 * @section LICENSE
 *
 * Open-WBO, Copyright (c) 2013-2017, Ruben Martins, Vasco Manquinho, Ines Lynce
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

#ifndef FormulaPB_h
#define FormulaPB_h
#include <cstdint>
#include <set>
#include <utility>
#include <vector>
#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif
#include <map>
#include <memory>
using NSPACE::vec;
using NSPACE::Lit;
#include "Model.h"
namespace openwbo {
    
  typedef std::map<int, std::string> indexMap;
    
  // Cardinality constraint of the form atMostK
  class Card {

  public:
    Card(vec<Lit> &lits, int64_t rhs, bool sign = false) {
      lits.copyTo(_lits);
      _rhs = rhs;
      if (sign) {
	int s = 0;
	for (int i = 0; i < _lits.size(); i++) {
	  s += 1;
	  _lits[i] = ~_lits[i];
	}
	_rhs = s - _rhs;
      }
      //     print();
    }

    Card() { _rhs = 0; }
    ~Card() {}

    void print() {
      printf("Card: ");

      for (int i = 0; i < _lits.size(); i++) {
	if (sign(_lits[i]))
	  printf("~");
	printf("%d ", var(_lits[i]) + 1);
      }
      printf(" <= %d\n", (int)_rhs);
    }
  
  
    void my_print(indexMap indexToName, bool original_vars = true) {
      // Assume _sign == false...
      printf("Card: ");

      for (int i = 0; i < _lits.size(); i++) {
	if (sign(_lits[i]))
	  printf("~");
	if(!original_vars)
	  printf("X%d ", var(_lits[i]) + 1);
	else
          if(indexToName.find(var(_lits[i])) != indexToName.end())
            printf("%s ", indexToName.at(var(_lits[i])).c_str());
          else
            printf("X%d ", var(_lits[i]) + 1);
      }
      printf(" <= %d\n", (int)_rhs);
    }

    vec<Lit> _lits;
    int64_t _rhs;
  };

  // PB constraint. The constraint sign is encoded in the structure.
  class PB {

  public:
    PB(vec<Lit> &lits, vec<uint64_t> &coeffs, int64_t rhs, bool s = false) {
      lits.copyTo(_lits);
      coeffs.copyTo(_coeffs);
      _rhs = rhs;
      _sign = s;
      for(int i = 0; i< coeffs.size(); i++) ub += coeffs[i];
    
      //     for(int i = 0; i< coeffs.size(); i++) printf("%lu\n", coeffs[i]);
      //     printf("%d\n" % ((s) ? 1 : 0));
      //     printf("rhs: %lu\n", rhs);
    }

    PB() {
      _rhs = 0;
      _sign = false;
      ub = 0;
    }
    ~PB() {}

    void addProduct(Lit l, int64_t c) {
      _coeffs.push();
      _lits.push();
      if (c >= 0) {
	_coeffs[_coeffs.size() - 1] = c;
	_lits[_lits.size() - 1] = l;
	ub += c;
      } else {
	_coeffs[_coeffs.size() - 1] = -c;
	_lits[_lits.size() - 1] = ~l;
	_rhs += -c;
	ub += -c;
      }
    }

    void addRHS(int64_t rhs) { _rhs += rhs; }

    void changeSign() {
      int s = 0;
      for (int i = 0; i < _coeffs.size(); i++) {
	s += _coeffs[i];
	_lits[i] = ~(_lits[i]);
      }
      _rhs = s - _rhs;
      _sign = !(_sign);
    }

    bool isClause() {
      // Assume _sign == false...
      bool sign = _sign;
      if (_sign)
	changeSign();
      if (_rhs != 1) {
	if (_sign != sign)
	  changeSign();
	return false;
      }
      for (int i = 0; i < _coeffs.size(); i++) {
	if (_coeffs[i] != 1) {
	  if (_sign != sign)
	    changeSign();
	  return false;
	}
      }
      return true;
    }

    bool isCardinality() {
      // Assume _sign == false...
      bool sign = _sign;
      if (_sign)
	changeSign();
      for (int i = 0; i < _coeffs.size(); i++) {
	if (_coeffs[i] != 1) {
	  if (_sign != sign)
	    changeSign();
	  return false;
	}
      }
      return true;
    }

    void print() {
      // Assume _sign == false...
      if (isClause())
	printf("Clause: ");
      else if (isCardinality())
	printf("Card: ");
      else
	printf("PB: ");

      for (int i = 0; i < _coeffs.size(); i++) {
	printf("%d ", (int)_coeffs[i]);
	if (sign(_lits[i]))
	  printf("~");
	printf("%d ", var(_lits[i]) + 1);
      }
      if(!_sign)
        printf(" >= %d\n", (int)_rhs);
      else
        printf(" <= %d\n", (int)_rhs);
    }

    void my_print(indexMap indexToName, bool original_vars = true) {
      // Assume _sign == false...
      if (isClause())
	printf("c\tClause:\t");
      else if (isCardinality())
	printf("c\tCard:\t");
      else
	printf("c\tPB:\t");

      for (int i = 0; i < _coeffs.size(); i++) {
	printf("%d ", (int)_coeffs[i]);
	if (sign(_lits[i]))
	  printf("~");
	if(!original_vars)
	  printf("y%d ", var(_lits[i]) + 1);
	else
	  printf("%s ", indexToName.at(var(_lits[i])).c_str());
      }
    
    
      if(!_sign)
        printf(" >= %d\n", (int)_rhs);
      else
        printf(" <= %d\n", (int)_rhs);
    }
  
  
    uint64_t getUB(){ return ub;}
  
    vec<uint64_t> _coeffs;
    vec<Lit> _lits;
    int64_t _rhs;
    bool _sign; // atLeast: false; atMost: true
    uint64_t ub;
  };

  class PBObjFunction {

  public:
    PBObjFunction(const vec<Lit> &lits, const vec<uint64_t> &coeffs, int64_t c = 0, int64_t factor = 1) {
      _lb = 0;
      _ub = 0;
      min_coeff = 0;
      max_coeff = 0;
      _const = c;
      _factor = factor;
      lits.copyTo(_lits);
      coeffs.copyTo(_coeffs);

      if(coeffs.size()){
	min_coeff = _coeffs[0];
	max_coeff = min_coeff;
	if(_coeffs[0] > 0)
	  _ub = min_coeff;
      }
      
      for(int i = 1; i < _coeffs.size(); i++){
	if(_coeffs[i] < min_coeff)
	  min_coeff = _coeffs[i];
	if(_coeffs[i] > max_coeff)
	  max_coeff = _coeffs[i];
	if(_coeffs[i] > 0)
	  _ub += _coeffs[i];
	else _lb +=_coeffs[i];
      }
      _ub *= _factor;
      _lb *= _factor;
      factorize();
      compute_sparse();
    }
    PBObjFunction(PBObjFunction&& other){
      _lits = std::move(other._lits);
      _coeffs = std::move(other._coeffs);
      _ub = other._ub;
      _lb = other._lb;
      _const = other._const;
      _factor = other._factor;
      max_coeff = other.max_coeff;
      min_coeff = other.min_coeff;
  
      other._ub = 0;
      other._lb = 0;
      other._const = 0;
      other._factor = 0;
      other.max_coeff = 0;
      other.min_coeff = 0;
      other.sparse(false);
      _sparse=other.sparse();
    }
    PBObjFunction(const PBObjFunction& other){*this = other;}
    PBObjFunction& operator=(const PBObjFunction& other){
      _lits.clear();
      _coeffs.clear();
      other._lits.copyTo(_lits);
      other._coeffs.copyTo(_coeffs);
      _ub = other._ub;
      _lb= other._lb;
      max_coeff = other.max_coeff;
      min_coeff = other.min_coeff;
      _const = other._const;
      _factor = other._factor;
      _sparse = other._sparse;
      return *this;
    }
    PBObjFunction& operator=(PBObjFunction&& other){
      _lits.clear();
      _coeffs.clear();
      _lits = std::move(other._lits);
      _coeffs = std::move(other._coeffs);
      _ub = other._ub;
      _lb= other._lb;
      max_coeff = other.max_coeff;
      min_coeff = other.min_coeff;
      _const = other._const;
      _factor = other._factor;
      _sparse = other.sparse();
      other._ub = 0;
      other._lb = 0;
      other.min_coeff = 0;
      other.max_coeff = 0;
      other._const = 0;
      other._factor = 0;
      other.sparse(false);
      return *this;
    }
    PBObjFunction():_sparse{}{
      _const = 0; _ub = 0;_lb = 0; min_coeff = 0; max_coeff = 0;
    }
    ~PBObjFunction() {}

    void addProduct(Lit l, int64_t c) {
      _coeffs.push();
      _lits.push();
      if (c >= 0) {
        _coeffs[_coeffs.size() - 1] = c;
        _lits[_lits.size() - 1] = l;
      } else {
        _coeffs[_coeffs.size() - 1] = -c;
        _lits[_coeffs.size() - 1] = ~l;
        _const += c;
      }
      //         printf("update _const: %ld\n", _const);
    }

    vec<uint64_t> _coeffs;
    vec<Lit> _lits;
    int64_t _const = 0;
    int64_t _factor = 1;
    uint64_t min_coeff;
    uint64_t max_coeff;
    uint64_t ub() const{ return _ub; }
    uint64_t lb() const{ return _lb; }
    uint64_t ub(uint64_t ubb) { auto old = ub(); _ub = ubb; return old; }
    uint64_t lb(uint64_t lbb) { auto old = lb(); _lb = lbb; return old; }
    void my_print(indexMap indexToName, bool original_vars = true, int maxsize=1000000) const {
      // Assume _sign == false...
      printf("c function report\n");
      if(maxsize < 0)
	maxsize = _coeffs.size();
      for (int i = 0; i < maxsize && i < _coeffs.size(); i++) {
	printf("%lu ", _coeffs[i]);
	if (sign(_lits[i]))
	  printf("~");
	if(!original_vars)
	  printf("y%d ", var(_lits[i]) + 1);
	else
	  if(indexToName.find(var(_lits[i])) != indexToName.end())
	    printf("%s ", indexToName.at(var(_lits[i])).c_str());
	  else
	    printf("Y%d ", var(_lits[i]) + 1);
      }
      if(_coeffs.size() > maxsize)
	printf("... (%d lits)", _coeffs.size());
      printf("\n");
        
      double tweight = 0;
      
      for (int i = 0; i < _coeffs.size(); i++)
	tweight += _coeffs[i];
      printf("c obj const: %ld\n", _const);
      printf("c obj factor: %ld\n", _factor);
      printf("c obj min coeff: %ld\n", min_coeff);
      printf("c obj max coeff: %ld\n", max_coeff);
      printf("c obj total coeff: %ld\n",(int64_t)tweight);
      printf("c obj nvars: %d\n", _coeffs.size());
      printf("c obj sparse: %d\n", (int)sparse());
    }
    operator bool(){
      return !empty();  
    }
    // note that two functions may match even if the constant
    // term is different
    bool operator==(const PBObjFunction& other){
      auto n = _lits.size();
      //must use signed ints. Imagine what would happen if the *this
      //was the null function.
      std::map<Lit, int64_t> terms{};
      //add first to 0,
      for(int i = 0; i < n; i++){
	auto sign = Glucose::sign(_lits[i]);
	auto lit = _lits[i];
	int64_t coeff = _coeffs[i]*_factor;
	if(sign){
	  lit = ~lit;
	  coeff = - coeff;
	}
	terms[lit]+=coeff*_factor;
      }
      //subtract other,
      for(int i = 0, n = other._coeffs.size(); i < n; i++){
	auto sign = Glucose::sign(other._lits[i]);
	auto lit = other._lits[i];
	int64_t coeff = other._coeffs[i]*_factor;
	if(sign) {
	  lit = ~lit;
	  coeff = -coeff;
	}
	terms[lit]-=coeff*other._factor;
      }
      for(auto& el: terms)
	if(el.second!=0) return false;
      return true;
    }
    // flashes out gcd of coefficients and updates _factor
    // accordingly. Returns true if _factor changes
    bool factorize(){
      auto n = _coeffs.size();
      if(!n)
	return false;
      std::set<uint64_t> coeffs{};
      std::map<uint64_t, uint64_t> factors{};
      // store coeffs into ordered map
      for(decltype(n) i = 0; i < n; i++)
	coeffs.insert(_coeffs[i]);
      { // compute factors
	uint64_t cur = *coeffs.rbegin();
	if(cur == 1)
	  return false;

	uint64_t d = 2;
	while( d*d <= cur){
	  if(cur % d == 0){
	    factors[d]++;
	    cur /= d;
	  }else 
	    d++;
	}
	factors[cur]++;

      }


      uint64_t gcd = 1;
      { // compute gcd
	for(const auto& coeff: coeffs)
	  for(auto& factor: factors){
	    uint64_t n = 0;
	    auto cur = coeff;
	    while(n < factor.second && cur % factor.first == 0){
	      n++;
	      cur /= factor.first;
	    }
	    factor.second = n;
	  }
   
	for(auto& factor: factors)
	  gcd *= pow(factor.first,factor.second);
      }
    
      if(gcd != 1){
	for(decltype(n) i = 0; i < n; i++)
	  _coeffs[i]/=gcd;
	_factor *= gcd;
	_ub /= gcd;
	_lb /= gcd;
	max_coeff /= gcd;
	min_coeff /= gcd;
	return true;
      }
      return false;
    }
    bool empty(){
      return _coeffs.size() == _lits.size() && (_lits.size() == 0);
    }
    void sparse(bool sparseo){_sparse=sparseo;}
    bool sparse() const {return _sparse;}
    bool compute_sparse();
    int64_t evaluate(const Model& model, bool denorm=true) const{
      uint64_t res = 0;
      for(int i = 0; i < _lits.size(); i++){
	auto l = _lits[i];
	assert(var(l) < (int)model.size());
	if ((sign(l) && model[var(l)] == l_False) || 
	    (!sign(l) && model[var(l)] == l_True)) {
	  res += _coeffs[i];
	  //                 printf(" %cx%d", (sign(l)) ? '-':' ', var(l)+1);
	}
      }
      if(denorm)
	return denormalize(res);
      else return res;
    }
    int64_t unscale(int64_t aff) const{ return aff/_factor;};
    int64_t scale(uint64_t lin) const{ return  lin *_factor;};
    int64_t unshift(int64_t aff) const{ return aff - _const;};
    int64_t shift(uint64_t lin) const{ return  lin + _const ;};

    // 'normalize' and 'denormalize' are inverse functions. 'normalise'
    // transforms affine values into unitary-linear values.
    int64_t normalize(int64_t aff) const{ return (aff - _const)/_factor;};
    int64_t denormalize (uint64_t lin) const{ return  lin *_factor + _const ;};
  private:
    bool _sparse {false};
    // upper bound of normalized function
    uint64_t _ub;
    // lower bound of normalized function is 0
    uint64_t _lb;

  };
  std::unique_ptr<PBObjFunction>  add(const PBObjFunction* pb, const PBObjFunction* pbb);
} // namespace openwbo

#endif
