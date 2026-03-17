#include "Converter.h"
#include "FormulaPB.h"
#include "MOCOFormula.h"
#include "ParserPB.h"
#include "algorithms/Alg_PBtoCNF.h"
#include "mtl/Vec.h"
#include "partition.h"
#include <iterator>
#include <memory>

namespace converter{
  openwbo::MOCOFormula MaxSATtoMOCO::convert(openwbo::MaxSATFormula& msf){
    Glucose::vec<Lit> lits;
    Glucose::vec<uint64_t> weights;
    for(auto i = 0, n = msf.nSoft(); i < n; i++){
      auto const& soft = msf.getSoftClause(i);
      lits.push(soft.assumption_var);
      weights.push(soft.weight);
    }
    
    openwbo::MOCOFormula mf{&msf};
    mf.addObjFunction(openwbo::PBObjFunction{lits,weights});
    return mf;
  }

  openwbo::MOCOFormula MOCOtoBrokenMOCO::convert(openwbo::MOCOFormula &mf, int par){
    vector<PBObjFunction> objs{};
    vector<partition::MyPartition> partitions{};
    int nObj = mf.nObjFunctions();
    partitions.reserve(nObj);
    for(int i = 0; i < nObj; i++ ){
      vector<PBObjFunction> tmp_objs{};
      PBObjFunction pb {*mf.getObjFunction(i)};
      PBObjFunction tail{};
      partition::Partitioner exec{pb};
      for(int par = 0; par < 200; par++, exec.bump_param(), exec.reset_terms()){
	tmp_objs.clear();
	for(auto pb = exec.headUnitary(); pb; pb = exec.headUnitary()){
	  tmp_objs.push_back(std::move(pb));
	  if(tmp_objs.size() == 30)
	    break;
	}
	tail = exec.tail();
	if(!tail)
	  break;
      };
      if(tail)
	tmp_objs.push_back(std::move(tail));
      objs.insert(
		  objs.begin(), 
		  std::make_move_iterator(tmp_objs.begin()), 
		  std::make_move_iterator(tmp_objs.end()));
      for(auto& el: objs)
	el.my_print(mf.maxsat_formula()->getIndexToName(), true, 1000);
    }
    mf.resetObjFunction();
    for(auto& el:objs)
      mf.addObjFunction(el);
    return mf;
  }

}
