#include "Alg_MasterMO.h"
#include <functional>

bool PBtoCNFMasterMO::buildWorkFormula(){
  updateMOFormulation();
  return true;

}
