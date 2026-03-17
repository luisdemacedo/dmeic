#include "MOCOFormula.h"
#include "FormulaPB.h"
#include <memory>
#include <utility>

namespace openwbo{

  void MOCOFormula::loadFormula(){
    assert(msf);
    of.clear();
    for(int i = 0; i < msf->nObjFunctions(); i++){
      auto ofo = msf->getObjFunction(i);
      addObjFunction(*ofo);
    }
  }

  void MOCOFormula::addObjFunction(const PBObjFunction& ofo){
    //     objective_function = new PBObjFunction(of->_lits, of->_coeffs, of->_const);
    of.push_back(std::make_unique<PBObjFunction>(ofo));
  }

   PBObjFunction const* MOCOFormula::getObjFunction(int i) { return of.at(i).get(); } //AG
  void MOCOFormula::replaceObjFunction(int i, std::unique_ptr<PBObjFunction>&& new_pb) {
    of.at(i).swap(new_pb);
 } //AG

  std::vector<std::unique_ptr<PBObjFunction>> MOCOFormula::resetObjFunction(){
    return std::move(of);
  }

  const std::vector<std::unique_ptr<PBObjFunction>>& 
  MOCOFormula::setObjFunction(std::vector<std::unique_ptr<PBObjFunction>>&& off){
    of = std::move(off);
    return of;
  }

}
