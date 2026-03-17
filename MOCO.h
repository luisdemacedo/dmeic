/*!
 * \author Ruben Martins - ruben@sat.inesc-id.pt
 *
 * @section LICENSE
 *
 * MiniSat,  Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
 *           Copyright (c) 2007-2010, Niklas Sorensson
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

#ifndef MOCO_h
#define MOCO_h


#include "core/SolverTypes.h"
#include "mtl/Vec.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#ifdef SIMP
#include "simp/SimpSolver.h"
#else
#include "core/Solver.h"
#endif
#include "stdexcept"
#include "MaxSAT.h"
#include "MOCOFormula.h"
#include "MaxTypes.h"
#include "utils/System.h"
#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>
#include "Model.h"
using NSPACE::vec;
using NSPACE::Lit;


namespace openwbo {
  // point in the objective space
  using  YPoint = std::vector<uint64_t>;
  using namespace std;
  std::ostream& operator<<(std::ostream& os, const YPoint& mdl);
  // point in the decision space
  std::ostream& operator<<(std::ostream& os, const Model& mdl);
  void modelClause(Model&& mod, vec<Lit>& clause);
  void modelClause(Model&& mod, vec<Lit>& clause, const vector<bool>& filter);
  void modelClausePlus(Model&& mod, vec<Lit>& clause);

  void blockModel(Solver* sol, Model& mod);
  Model modelEmbed(const Model& mod, unsigned int nVars);

  class MOCO;
  // solutions found so far
  class Solution {

  public: class OneSolution{
    Model m;
    YPoint yp;
    bool ev=true;
    Solution* s = nullptr;
    int id = -1;
    ;//objective functions. Collected from
    //the MOCO problem
  public:
    OneSolution():m{} {}
    OneSolution(Solution* ss, const Model& mm):m{mm}, s{ss}, id{ss->id} {}
    OneSolution(Solution* ss, Model&& mm): m{std::move(mm)}, s{ss},id{ss->id} {}
    friend std::ostream& operator<<(std::ostream& os, OneSolution& mdl);
    void model(const Model& mm){m = mm;}
    void model(Model&& mm){m = std::move(mm);}
    void eval(bool evv){ev = evv;}
    const Model& model() const{return m;}
    const bool eval() const{return ev;}
    const YPoint& yPoint();
    bool comparable(OneSolution osol){return s = osol.s; };
    void compute(){};
  };


  public:
    using notes_t = int;
    using element_t = std::pair<OneSolution,notes_t>;
  private:
    int id = 0;
    int barred = 0;
    int dropped = 0;
    MOCO* maxs;
    using container_t = std::map<int, element_t>;
    container_t mods;
    element_t& element(int i){
      if(i==-1) return (--mods.end())->second;
      if(i < id){
	return mods[i];
      }
      throw std::out_of_range("no such element in Solution...");      
    }
  public:
    Solution(MOCO* maxss):mods(){maxs = maxss;};
    container_t::iterator begin(){return mods.begin();}
    container_t::iterator end(){return mods.end();}
    const container_t::const_iterator begin() const {return cbegin();}
    const container_t::const_iterator end() const {return cend();}
    const container_t::const_iterator cbegin() const {return mods.begin();}
    const container_t::const_iterator cend() const {return mods.end();}
    
    notes_t& note(int i=-1){return element(i).second;}
    const YPoint& yPoint(int i=-1){return element(i).first.yPoint();}
    const Model& model(int i=-1){return element(i).first.model();}
    const OneSolution& oneSolution(int i=-1){return element(i).first;}
    OneSolution wrap(const Model& m){return OneSolution{this, m};}
    void pop();
    void push(const Model& m, notes_t notes=-1);
    void push(Model&& m, notes_t notes=4);
    void push(OneSolution& m, notes_t notes=0);
    void push(element_t&& element){mods[id++]=element;}
    // by default makes sure the result is free of inter-dominances,
    // check toggles testing the new element, filter toggles testing
    // the set.
    bool pushSafe(const Model& m, notes_t notes = 0, bool check = true, bool filter = true );
    void clear(){mods.clear(); id = 0;}
    typename container_t::iterator remove(const typename container_t::iterator it){return mods.erase(it);}
    size_t size(){return mods.size();};
    shared_ptr<MOCOFormula> maxsat_formula();
    void report();
  };

  // generalized MOCO instance. In particular, it might actually be
  // a MOCO instance
  class MOCO: public MaxSAT {
  public:
    MOCO(): MaxSAT{}, lowerBound(this), 
	    wf{make_shared<MOCOFormula>(new MaxSATFormula{})}{}
    ~MOCO(){
      for(size_t i = 0; i < effsols.size(); i++)
        delete [] effsols[i].first;
    
      for(size_t i = 0; i < nondom.size(); i++)
        delete [] nondom[i];
        
      for(size_t i = 0; i < LBset.size(); i++)
        delete [] LBset[i];
    
      effsols.clear();
      nondom.clear();
      LBset.clear();
    }
    MOCO(MOCO&& other) = default;
    MOCO& operator=(MOCO&&) = default;
    Solution lowerBound;
    std::shared_ptr<MOCOFormula> mf;
    std::shared_ptr<MOCOFormula> wf;
    shared_ptr<MOCOFormula> getFormula(){return mf;};    
    shared_ptr<MOCOFormula> getWFormula(){return wf;};
    virtual shared_ptr<MOCOFormula> workFormula(){return getWFormula();};
    void shareFormula(shared_ptr<MOCOFormula> mff);
    void loadFormula(MaxSATFormula *maxsat) override;
    Solution& solution() {return sol;}
    Solution sol{this};
    //evaluates model. must use maxsat_formula 
    YPoint evalModel(const Model& m);
    //temporary: move solutions to nondom:
    void transferToSolution();
    //temporary: move solutions to nondom:
    void transferToEffSols();
    void printAnswer(int type) override;
    void printEffSolutions(bool printLBset);
    void printMyStats();
    void saveEfficientSol(const vec<lbool> &currentModel, const uint64_t * point, bool filter=false);
    void printApproxRatio();
    bool updateLowerBoundSet(const uint64_t * point, bool filter, uint64_t ireencode=1);
    void clearLowerBoundSet(uint64_t excepti=0);
    void updateExpectedApprox(double eps, uint64_t lbi){printf("c updateExpectedApprox\n"); expepsilon = eps; lbseti_expeps = lbi;};
    virtual Model make_model(const vec<lbool>&);
  protected:
    // bounds::BoundsCalculator calc;
    std::vector<uint64_t *> nondom;
    std::vector<uint64_t *> LBset; //para as versoes approximadas
    //model (sol space) and index of nondom point to the corresponding point (in obj space)
    std::vector<std::pair<lbool *, uint64_t>> effsols;

  };

  uint64_t hv(const YPoint& yp);
  uint64_t hv_nonzero(const YPoint& yp);
} // namespace openwbo

#endif
