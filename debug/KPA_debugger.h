#ifndef KPA_DEBUGGER_H
#define KPA_DEBUGGER_H
#include "core/Solver.h"
#include "../encodings/Enc_KPA.h"
#include "../MOCO.h"
#include "../FormulaPB.h"
#include "debug.h"
#include <cstdint>

namespace debug{
  // build one solution, given the value of the model 
  using namespace openwbo;
  using namespace encoding;
  class KPA_debugger{
    KPA& kpa;
    PBObjFunction pb;
    Glucose::Solver* solver;
  public:
    KPA_debugger(Solver* so, KPA& ko): kpa{ko}, pb{}, solver{so}{}
    void print_one_solution(const Model& m);
    Model print_one_solution(uint64_t nvars);
    Model print_one_solution(uint64_t nvars, vec<Lit>& assumptions);
    Model print_one_solution(vec<Lit>& assumptions);
  };
  KPA_debugger make_KPA_debugger(Solver*);

}

#endif
