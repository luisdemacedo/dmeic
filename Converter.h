#ifndef CONVERTER_H
#define CONVERTER_H

#include "MaxSATFormula.h"
#include "MOCOFormula.h"
#include "partition.h"
#include <memory>
#include <cstdint>

namespace converter {
  class MaxSATtoMOCO{
  public:
    openwbo::MOCOFormula convert(openwbo::MaxSATFormula& msf);
  };

  class MOCOtoBrokenMOCO{
  public:
    openwbo::MOCOFormula convert(openwbo::MOCOFormula& mf, int par);
    
  };
}



#endif
