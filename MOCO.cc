#include "MOCO.h"
#include "MOCOFormula.h"
#include "core/SolverTypes.h"
#include <memory>
#include <float.h>
#include <iostream>
//does a weakly dominates b?
bool wdominates(const uint64_t * a, const uint64_t * b, int d){
    for(int i = 0; i < d; i++)
        if(a[i] > b[i])
            return false;
    return true;
}
namespace openwbo {
  void MOCO::loadFormula(MaxSATFormula *maxsat){
    MaxSAT::loadFormula(maxsat);
    mf = std::make_shared<openwbo::MOCOFormula>(maxsat);
    // calc = bounds::BoundsCalculator{maxsat};
    mf->loadFormula();
  }

  YPoint MOCO::evalModel(const Model& model){
      
    YPoint yp(getFormula()->nObjFunctions());
    if(model.size()){
      for(int di = 0; di < getFormula()->nObjFunctions(); di++){
	const auto& objf = getFormula()->getObjFunction(di);
	auto entry = objf->evaluate(model, false);
	yp[di] = objf->scale(entry);
      }
    }
    return yp;
  }

  void Solution::pop(){
    mods.erase(--mods.end());
  }
  void Solution::report(){
    std::cout<<"c Solution report"<<endl;
    std::cout<<"c barred:"<< this->barred << endl;
    std::cout<<"c dropped:"<< this->dropped << endl;
    std::cout<<"c size:"<< this->size() << endl;
    // for(int i = 0, n = this->size(); i < n; i++)
    //   std::cout << "c "<< yPoint(i) << endl;
    std::cout<<"c Solution report done"<<endl;
  }

  void MOCO::transferToEffSols(){
    int no = getFormula()->nObjFunctions();
    uint64_t point[getFormula()->nObjFunctions()];  
    vec<lbool> vm;
    auto & sol= *(--solution().end());
    sol.second.first.model().copyTo(vm);
    YPoint yp = sol.second.first.yPoint();
    for(int i = 0; i < no; i++ )
      point[i] = yp[i];
    saveEfficientSol(vm, point);
  }

  void MOCO::transferToSolution(){
    solution().push(Model{(*(--effsols.end())).first, (Model::size_type)getFormula()->nVars()});
  }
  shared_ptr<MOCOFormula> Solution::maxsat_formula(){
    return maxs->getFormula();
  }

  // Prints the corresponding answer.
void MOCO::printAnswer(int type) {
  consolidateSolution();
  while(solution().size()){
    transferToEffSols();
    solution().pop();
  }
  if (verbosity > 0 && print)
    printStats();
  printf("c ---------- OUTPUT ---------------\n");

  if (type == _UNKNOWN_ && effsols.size() > 0)
    type = _SATISFIABLE_;
  else if (type == _UNKNOWN_ && model.size() > 0)
    type = _SATISFIABLE_;

  // store type in member variable
  searchStatus = (StatusCode)type;
  if(!print) return;

  switch (type) {
  case _SATISFIABLE_:
    printf("s SATISFIABLE\n");
    clearLowerBoundSet(lbseti_expeps); //remove the last (incomplete) LBset
    printEffSolutions(true);
    printMyStats();
    fflush(stdout);
    break;
  case _OPTIMUM_:
    printf("s OPTIMUM\n");
    printEffSolutions(true);
    printApproxRatio();
    printMyStats();
    fflush(stdout);
    break;
  case _UNSATISFIABLE_:
    printf("s UNSATISFIABLE\n");
    break;
  case _UNKNOWN_:
    printf("s UNKNOWN\n");
    break;
    
  case _MEMOUT_:
    printEffSolutions(false);
    printMyStats();
    printf("s MEMOUT\n");
    fflush(stdout);
    break;
  default:
    printf("c Error: Invalid answer type.\n");
  }
  exit(0); //AG - adicionei depois usar o setrlimit para ignorar o tempo de encoding (em Alg_BLS.cc)
}

  void MOCO::printEffSolutions(bool printLBset) {
    
    if(effsols.size() == 0)
      return;

    int d = maxsat_formula->nObjFunctions();

  
    FILE * f = stdout;
    if(print_model){
      //     std::stringstream s;
      f = (print_my_output) ? fopen (effsols_file,"w") : stdout;
      //         printf("c print to %s? %d\n", effsols_file, print_my_output);
      for(size_t j = 0; j < effsols.size(); j++){
	lbool *modelj = effsols[j].first;
            
	//             s << "v ";
	if(!print_my_output)
	  fprintf(f, "v");
            
	if (maxsat_formula->getFormat() == _FORMAT_PB_) {
	  for (int i = 0; i < maxsat_formula->nVars(); i++) {
	    indexMap::const_iterator iter = maxsat_formula->getIndexToName().find(i);
	    if (iter != maxsat_formula->getIndexToName().end()) {
	      fprintf(f, " ");
	      if (modelj[i] == l_False)
		//                         s << "-";
		fprintf(f, "-");
	      else
		fprintf(f, " ");;
	      fprintf(f, "%s", iter->second.c_str());
	      //                         s << iter->second.c_str() << " ";
	    }
	  }
	} else {
	  for (int i = 0; i < maxsat_formula->nVars(); i++) {
	    if (modelj[i] == l_True)
	      //                     s << i+1 << " ";
	      fprintf(f, "  %d", i+1);
	    else
	      //                     s << -(i+1) << " ";
	      fprintf(f, " -%d", i+1);
	  }
	}
	//             printf("%s", s.str().c_str());
	fprintf(f, "\n");
      }
        
      if(print_my_output) fclose(f);
    }
    printf("c %lu (efficient) solutions\n", effsols.size());
    
    
    //     if(!print_my_output){
    printf("c ------- \n");
    printf("c pts of transformed prob\n");
    for(size_t i = 0; i < nondom.size(); i++){
      printf("c pt");
      for(int di = 0; di < d; di++){
	printf(" %ld", nondom[i][di]); // + maxsat_formula->getObjFunction(di)->_const);
      }
      printf("\n");
    }
    printf("c ------- \n");
    printf("c %lu points T\n", nondom.size());
    printf("c ------- \n");
        
    printf("c lower bound set of transformed prob\n");
    for(size_t i = 0; i < LBset.size(); i++){
      printf("c lb");
      for(int di = 0; di < d; di++){
	printf(" %ld", LBset[i][di]); // + maxsat_formula->getObjFunction(di)->_const);
      }
      printf("\n");
    }
    printf("c ------- \n");
    printf("c %lu lbs T\n", LBset.size());
    printf("c ------- \n");
    //     }
    
    //Lower bound set
    //     if(!print_my_output){
    if(printLBset && LBset.size() > 0){
      f = (print_my_output) ? fopen (lbset_file,"w") : stdout;
      for(size_t i = 0; i < LBset.size(); i++){
	if(!print_my_output) fprintf(f, "c LBs");
	for(int di = 0; di < d; di++){
	  fprintf(f, " %ld", LBset[i][di] + maxsat_formula->getObjFunction(di)->_const); // + maxsat_formula->getObjFunction(di)->_const);
	}
	fprintf(f, "\n");
      }
      //         fprintf(f, "c ------- \n");
      printf("c %lu points in lower bound set\n", LBset.size());
      printf("c ------- \n");
      if(print_my_output) fclose(f);
    }
 
    
    // Solutions (objective space)
    f = (print_my_output) ? fopen (objv_file,"w") : stdout;
    
    for(size_t i = 0; i < nondom.size(); i++){
      if(!print_my_output)
	fprintf(f, "o");
      for(int di = 0; di < d; di++){
	fprintf(f, " %ld", nondom[i][di] + maxsat_formula->getObjFunction(di)->_const);
      }
      fprintf(f, "\n");
    }
    printf("c %lu nondominated points\n", nondom.size());
    if(print_my_output) fclose(f);

    printf("c _consts:");
    
    for(int di = 0; di < d; di++){
      printf(" %ld", maxsat_formula->getObjFunction(di)->_const);
    }
    printf("\n");
  }
void MOCO::printMyStats(){
    double totalTime = cpuTime();
//     Solver *S = getSolver();
//     printf("-> %d , nondom.size(): %d\n", _nnondom_, nondom.size());
    runstats[_nsatcalls_] = nbSatisfiable;
    runstats[_ncalls_] = nbSatCalls;
    runstats[_neffsols_] = effsols.size();
    runstats[_nnondom_] = nondom.size();
//     
//     if(S != NULL){
//         printf("Solver is not null!\n");
//         printf("clauses solver, formula: %d %d\n", S->nClauses(), maxsat_formula->nHard());
//         runstats[_nencvars_] = S->nVars() - maxsat_formula->nVars();
//         runstats[_nencclauses_] = S->nClauses() - maxsat_formula->nHard();  
//     }
    runstats[_nprobvars_] = maxsat_formula->nVars();
    runstats[_nprobclauses_] = maxsat_formula->nHard();
    
    updateStats(); //update info about encoding size

    printf("clrunstats %18s %12s %12s %12s %12s %12s %18s %12s %18s %18s %8s %8s %8s\n", "nsatcalls_1stSol", "nsatcalls", "ncalls", "n_eff_sols", "n_nondom", "n_prob_vars", "n_prob_clauses", "n_enc_vars(sum)", "n_enc_clauses(sum)", "n_enc_rootvars(sum)", "n_reencodes", "rapprox", "nobj");
    printf("crunstats %12d %12d %12d %12d %12d %12d %18d %12d %18d %18d %18d %18.4f %10d\n", runstats[_nsatcalls1stSol_], runstats[_nsatcalls_], runstats[_ncalls_], runstats[_neffsols_], runstats[_nnondom_], runstats[_nprobvars_], runstats[_nprobclauses_], runstats[_nencvars_], runstats[_nencclauses_], runstats[_nencrootvars_], runstats[_nreencodes_], repsilon, getFormula()->nObjFunctions());
//     
//     
    timestats[_totaltime_] = totalTime - initialTime;
    printf("cltimestats %12s %12s\n", "time_1stSol", "totaltime");
    printf("ctimestats %12.2f %12.2f\n", timestats[_time1stSol_], timestats[_totaltime_]);
    
    if(print_my_output){
        FILE * file = fopen (stats_file,"w");
        fprintf(file, "# %18s %12s %12s %12s %12s %12s %18s %12s %18s %18s", "nsatcalls_1stSol", "nsatcalls", "ncalls", "n_eff_sols", "n_nondom", "n_prob_vars", "n_prob_clauses", "n_enc_vars", "n_enc_clauses", "n_enc_rootvars");
        fprintf(file, " %12s %12s %12s\n", "rapprox", "time_1stSol", "totaltime");
        
        fprintf(file, "  %18d %12d %12d %12d %12d %12d %18d %12d %18d %18d", runstats[_nsatcalls1stSol_], runstats[_nsatcalls_], runstats[_ncalls_], runstats[_neffsols_], runstats[_nnondom_], runstats[_nprobvars_], runstats[_nprobclauses_], runstats[_nencvars_], runstats[_nencclauses_], runstats[_nencrootvars_]);
        fprintf(file, " %12.4f %12.2f %12.2f\n", repsilon, timestats[_time1stSol_], timestats[_totaltime_]);
    
        fclose(file);
    }
}
void MOCO::saveEfficientSol(const vec<lbool> &currentModel, const uint64_t * point, bool filter){
    
    double runtime = cpuTime();
    lbool * model;
    uint64_t * pt;
    int d = maxsat_formula->nObjFunctions();
    
#ifdef __DEBUG__
    printf("c time (of new solution): %.3f\n", runtime-initialTime);
#endif
    if(nondom.size() == 0){
        timestats[_time1stSol_] = runtime-initialTime;
        runstats[_nsatcalls1stSol_] = nbSatisfiable;
    }
    //TODO FIX: Na versao de enumerar todas as eficientes, a relacao entre nondom e effsols sera de 1-N
    bool isdom = false;
    if(filter){
        //estou a assumir que nenhum ponto em nondom domina o point, mas o point pode dominar pontos
        //em nondom
        for(size_t i = 0; i < nondom.size() && !isdom; i++){
            if(wdominates(nondom[i], point, d)){
                printf("c point is dominated by:\n");
                for(int j = 0; j < d; j++)
                    printf(" %lu", nondom[i][j]);
                printf("\n");
                
//                 exit(-1);
                isdom = true;
            }else if(wdominates(point, nondom[i], d)){
                delete [] nondom[i];
                delete [] effsols[i].first;
                nondom.erase(nondom.begin()+i);
                effsols.erase(effsols.begin()+i);
                i--;
            }
        }
    }
    if(!isdom){
        pt = new uint64_t[d];
        for(int i = 0; i < d; i++) 
            pt[i] = point[i];
        nondom.push_back(pt);
        
        // Only store the value of the variables that belong to the original MaxSAT formula.
        model = new lbool[maxsat_formula->nVars()];
        for (int i = 0; i < maxsat_formula->nVars(); i++){
            model[i] = currentModel[i];
        //     printf(" %d", model[i]);
        }
        //   printf("\n"); 
        
        effsols.push_back(std::pair<lbool *, uint64_t>(model, nondom.size()-1));
    }
//   printEffSolutions();
//     printf("archive size: %lu\n", nondom.size());
}

//na pratica, isto calcula o multiplicative epsilon indicator, em que o LBset e o conjunto de
//referencia e o nondom e o conjunto a ser avaliado
void MOCO::printApproxRatio(){
    int d = maxsat_formula->nObjFunctions();
    double realeps = 0;
    double pteps, ptepsi;
    for(size_t j = 0; j < LBset.size(); j++){
        pteps = DBL_MAX;
        for(size_t i = 0; i < nondom.size(); i++){
            ptepsi = 1;
            for(int di = 0; di < d; di++){
                //se nao for 0 (se for, entao o valor acima e' o minimo da funcao objectivo di)
                //e se o racio for maior do que dos outros di
                //e se LB*epsilon > LB+1 (para contemplar os casos em que epsilon_approx < 2)
                if(LBset[j][di] >  0 && float(nondom[i][di])/LBset[j][di] > ptepsi && LBset[j][di]*(float(nondom[i][di])/LBset[j][di]) >= LBset[j][di]+1){
                    ptepsi = float(nondom[i][di])/LBset[j][di];
                }
            }
//             printf("%f < %f ?\n", ptepsi, pteps);
            if(ptepsi < pteps) pteps = ptepsi;
        }
        if(pteps > realeps) realeps = pteps;
    }
    
    printf("c ------- \n");
    printf("c observed and expected approx ratio\n");
    printf("c tapprox <= %.4f\n", realeps); //real approximation ratio
    printf("c eapprox <= %.4f\n", expepsilon);
    printf("c ------- \n");
    repsilon = realeps;
}
//returns true if 'point' was successfully added
bool MOCO::updateLowerBoundSet(const uint64_t * point, bool filter, uint64_t ireencode){
    uint64_t * pt;
    
    int d = maxsat_formula->nObjFunctions();
        /*
    printf("add LB (it: %lu): ", ireencode);
    for(int i = 0; i < d; i++)
        printf(" %lu", point[i]);
    printf("\n");*/
    
    bool isdom = false;
    if(filter){
        for(size_t i = 0; i < LBset.size(); i++){
            if(LBset[i][d] == ireencode && wdominates(LBset[i], point, d)){
                isdom = true;
            }else
            if((LBset[i][d] + 2 <= ireencode) || (LBset[i][d] == ireencode && wdominates(point, LBset[i], d))){
                delete [] LBset[i];
                LBset.erase(LBset.begin()+i);
                i--;
            }
        }
    }
    if(!isdom){
        pt = new uint64_t[d+1]; //a ultima posicao guarda a iteracao de reencode em que foi adicionado
        for(int i = 0; i < d; i++) 
            pt[i] = point[i];
        pt[d] = ireencode; //augmented
        LBset.push_back(pt);
    }
    return !isdom; 
}




void MOCO::clearLowerBoundSet(uint64_t excepti){
    if(excepti <= 0){
        for(size_t i = 0; i < LBset.size(); i++)
            delete [] LBset[i];
        LBset.clear();
    }else{
        int d = maxsat_formula->nObjFunctions();
        for(size_t i = 0; i < LBset.size(); i++){
            if(LBset[i][d] != excepti){
                delete [] LBset[i];
                LBset.erase(LBset.begin()+i);
                i--;
            }
        }
        
    }
}

  Model MOCO::make_model(const vec<lbool>& m){
    vec<lbool> proj;
    for(int i = 0, n = maxsat_formula->nInitialVars(); i < n; i++)
      proj.push(m[i]);
    return Model{m};
  }

  uint64_t hv_nonzero(const YPoint& yp){
    uint64_t x = 1;
    for(auto el: yp)
      x *= el + 1;
    return x;
  }

  uint64_t hv(const YPoint& yp){
    uint64_t x = 1;
    for(auto el: yp)
      x *= el;
    return x;
  }

  std::ostream& operator<<(std::ostream& os, Solution::OneSolution& mdl){
    os << "(" << mdl.id << ") " << mdl.yPoint();
    return os;
}

}
