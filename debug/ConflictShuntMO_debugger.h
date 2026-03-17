#ifndef CONFLICTSHUNTMO_DEBUGGER_H
#define CONFLICTSHUNTMO_DEBUGGER_H
#include "../algorithms/Alg_ConflictShuntMO.h"
#include "../algorithms/Alg_SlideDrillMO.h"
#include "../algorithms/Alg_HittingSetsStratMO.h"
#include <memory>
namespace debug {
  class ConflictShuntMO_debugger{
  public:
    ConflictShuntMO_debugger(): solver{}{
      upper = std::make_shared<UpperBoundHonerServerMO>();
      lower = std::make_shared<HittingSetsServerMO>();
      solver = ConflictShuntMO(std::move(lower), std::move(upper));
    }
  private:
    ConflictShuntMO solver;
    unique_ptr<ServerMO> upper{};
    unique_ptr<ServerMO> lower{};

};
  
    }

#endif
