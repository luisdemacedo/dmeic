#ifndef ALG_UNSATSATINCHSMO_H
#define ALG_UNSATSATINCHSMO_H
#include "./Alg_UnsatSatMO.h"

namespace openwbo{
  class UnsatSatIncHSMO: public UnsatSatIncMO{
  public:
    UnsatSatIncHSMO(int verb = _VERBOSITY_MINIMAL_, int weight = _WEIGHT_NONE_, int strategy = _WEIGHT_NONE_, 
		    int enc = _CARD_MTOTALIZER_, int pb = _PB_SWC_, 
		    int pbobjf = _PB_GTE_, int apmode = encoding::_ap_outvars_, float eps = 1, 
		    int searchStrat=3, float redFact=-1) : 
      PBtoCNF(verb, weight, strategy, enc, pb, pbobjf), 
      UnsatSatMO(verb, weight, strategy, enc, pb, pbobjf){}

    void build() override {
      PBtoCNFServerMO::build();
      solver = buildSolverMO();
      vector<uint64_t> doubles;
      for(int i = 0; i < getFormula()->nObjFunctions(); i++ ){
	const auto& lits =getFormula()->getObjFunction(i)->_lits;
	for(int j = 0, n = lits.size(); j < n; j++ ){
	  if(blockedVars.erase(~lits[j])){
	    doubles.push_back(var(lits[j]));
	    continue;
	    
	  }
	  blockedVars.insert(lits[j]);
	}
	for(auto& el: doubles){
	  auto lit = mkLit(el);
	  blockedVars.erase(lit);
	  blockedVars.erase(~lit);
	}
      }
}

    //RootLits are ready to be sliced
    bool buildWorkFormula() override  {
      bool result = UnsatSatIncMO::buildWorkFormula();
      // prepare slicing
      for(int i = 0; i < (int) objRootLits.size(); i++){
	auto old = *dynamic_cast<rootLits::RootLits*>(objRootLits[i].get());
	objRootLits[i] = 
	  std::make_shared<rootLits::RootLitsSliced>
	  (rootLits::RootLitsSliced{std::move(old), *getFormula()->getObjFunction(i)});
	// the complete objective functions were stored into
	// objRootLits. Their place is taken by the partial functions
	// from now on
	getFormula()->replaceObjFunction(i, 
					 std::make_unique<PBObjFunction>(PBObjFunction{}));
      }
      return result;
    }
    bool not_done() override {return true;}
    bool searchUnsatSatMO() override;
    // toggles sliced state of objRootLits. Requires that objRootLits
    //is a objRootLitsSliced subtype
    void toggleSlice(){
      for(auto& el: objRootLits){
	auto& sliced = *dynamic_cast<rootLits::RootLitsSliced*>(el.get());
	sliced.toggleSlice();
      } 
    }
    void forceSlice(bool b){
      for(auto& el: objRootLits){
	auto& sliced = *dynamic_cast<rootLits::RootLitsSliced*>(el.get());
	sliced.setSlice(b);
      } 
    }
    bool rootedSearch(const YPoint& yp) override;

    // updates the slice
    void incrementSlice(const partition::MyPartition::part_t&);
    void thaw(const set<Lit>& s);
    int assumeDominatingRegion(const YPoint& yp) override;
    bool extendUL(YPoint& ul) override;
    void checkSols() override;
    int blockStep(const YPoint& yp) override;
    const PBObjFunction& slicedObjective(int i);
    void increment() override;
  private:
    std::vector<std::unique_ptr<PBObjFunction>> off{}; 
    std::map<YPoint, Lit> soft_blocks;

  };
}

#endif
