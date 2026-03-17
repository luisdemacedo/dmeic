#include "KPA_debugger.h"
#include "core/SolverTypes.h"
#include <cstdint>

namespace debug{
  KPA_debugger make_KPA_debugger(Solver* so, KPA& ko){
    return KPA_debugger{so, ko};
  }
  
  Model KPA_debugger::print_one_solution(uint64_t nvars){
    auto assumptions = vec<Lit>(0);
    return print_one_solution(solver->nVars(), assumptions);    
  }

  Model KPA_debugger::print_one_solution(uint64_t nvars, vec<Lit>& assumptions){
    auto mod = Model{};
    if(solver->solveLimited(assumptions) == l_True){
      auto mod = debug::make_model(solver->model, nvars);
      print_one_solution(mod);
    }    
    else printf("not satisfiable\n");
    return mod;
  }

  Model KPA_debugger::print_one_solution(vec<Lit>& assumptions){
    return print_one_solution(solver->nVars(), assumptions);
  }

  void KPA_debugger::print_one_solution(const Model& mod){
    auto val = kpa.pb.evaluate(mod);
    printf("value of function is %lu\n", val);
    for(int i=0, n=kpa.ncoeffs-1;i<n;i++){
      printf("digit %d\n", i);
      for(int j=0, m=kpa.ls_szs[i];j<m;j++)
	printf("%d->%d: x%d\n",j , mod[kpa.ls[i][j]] == l_True, kpa.ls[i][j]);
    }
    return;
  }
  
}
