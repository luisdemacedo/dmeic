// #include "Alg_ParPMinimalMO.h"
// #include <algorithm> // std::max
//
// using namespace openwbo;
// // using namespace NSPACE;
// using NSPACE::toLit;
//
// void ParPMinimalMO::search_MO() {
//   // Init Structures
//   init();
//
//   //     printf("\t\tc strategic_search\n");
//   //     printf("c eps: %f\n", epsilon);
//
//   // Build solver
//   double epsthreshold = 1 + 1e-4;
//
//   buildSolversMO(omp_get_max_threads()); // TODO: maybe make this a parameter
//
//   bool resform, terminate = false;
//   nbMCS = 0;
//
//   answerType = _UNKNOWN_;
//
//   bool permanentBlock = false;
//
//   while (!terminate) {
//     // encode obj functions
//
//     resform = updateMOFormulationIfSAT();
//
//     if (resform) {
//
//       printf("c search\n");
// #pragma omp parallel num_threads(workers.size())
//       searchParPMinimalMO();
//     } else {
//       printf("c No more solutions!\n");
//     }
//     printf("c Done searching\n");
//     printf("c epsilon: %f\n", epsilon);
//     printf("c reductionFactor: %f\n", redFactor);
//     if ((permanentBlock && !resform) || epsilon <= 1 || redFactor < 0) {
//       terminate = true;
//       printf("c time to terminate\n");
//     } else {
//       if (epsilon <= epsthreshold)
//         epsilon = 1;
//       else
//         epsilon = 1 + (epsilon - 1) / redFactor;
//       printf("c REENCODE epsilon = %f\n", epsilon);
//     }
//   }
//
//   if (nondom.size() > 0) {
//
//     if (epsilon <= 1) {
//       printf("c LBset = PF\n");
//       clearLowerBoundSet();
//       for (size_t i = 0; i < nondom.size(); i++)
//         updateLowerBoundSet(nondom[i], false);
//     }
//     answerType = _OPTIMUM_;
//   } else {
//     int nreencodes = 0;
//     for (size_t i = 0; i < workers.size(); i++)
//       nreencodes += workers[i].nreencodes;
//     if (nreencodes == 1)
//       clearLowerBoundSet();
//   }
//
//   printAnswer(answerType);
// }
//
// bool ParPMinimalMO::searchParPMinimalMO() {
//   auto &w = workers[omp_get_thread_num()];
//   double runtime = cpuTime();
//   int nObj = maxsat_formula->nObjFunctions();
//
//   YPoint ul(nObj);
//   std::mt19937 rng(std::random_device{}());
//   std::uniform_real_distribution<double> dist(0.0, 1.0);
//
//   lbool sat;
//   do { // Randomizing the starting point
//     vec<Lit> core;
//     for (size_t i = 0; i < w.solver->conflict.size(); i++)
//       core.push(~w.solver->conflict[i]);
//
//     if (core.size())
//       w.solver->addClause(core);
//
//     w.assumptions.clear();
//     for (size_t i = 0; i < nObj; i++) {
//       for (size_t j = 0; j < getFormula()->getObjFunction(i)->_lits.size();
//       j++)
//         if (dist(rng) < 0.1)
//           w.assumptions.push(getFormula()->getObjFunction(i)->_lits[j]);
//     }
//   } while ((sat = solve(omp_get_thread_num())) != l_True &&
//            (sat == l_Undef || w.solver->conflict.size() > 0));
//
//   for (; sat == l_True;) {
//     for (; sat == l_True;) {
//       Model m = make_model(w.solver->model);
//       YPoint yp = evalModel(m);
//       // solution().pushSafe(m); Race condition, but we will fix it later
//       // ul = solution().yPoint();
//       // share solutions and clauses
//       //  blockDominatedRegion(ul);
//       std::ostringstream oss;
//       oss << ul;
//       std::osyncstream(std::cout) << "c o " << oss.str() << "\n";
//       runtime = cpuTime();
//       printf("c new feasible solution (time: %.3f)\n", runtime -
//       initialTime); w.assumptions.clear();
//       //     PBtoCNF::assumeDominatingRegion(ul);
//
//       //     shareClauses();
//       //     shareSolutions(true);
//       sat = solve(omp_get_thread_num());
//       while (sat == l_Undef) {
//         // shareClauses();
//         // shareSolutions(true);
//         sat = solve(omp_get_thread_num());
//       }
//     }
//     w.assumptions.clear();
//     runtime = cpuTime();
//     printf("c new optimal solution (time: %.3f)\n", runtime - initialTime);
//     //   blockDominatedRegion(ul);
//     //   shareClauses();
//     //   shareSolutions(true);
//     sat = solve(omp_get_thread_num());
//     while (sat == l_Undef) {
//       // shareClauses();
//       // shareSolutions(true);
//       sat = solve(omp_get_thread_num());
//     }
//   }
//
//   // if (solution().size() == 0) {
//   //   answerType = _UNSATISFIABLE_;
//   //   return false;
//   // } else {
//   //   answerType = _OPTIMUM_;
//   // }
//   return true;
// }
//
// void ParPMinimalMO::shareSolution(const openwbo::Solution::OneSolution osol)
// {
//   auto &w = workers[omp_get_thread_num()];
//   std::vector<openwbo::Solution::OneSolution> receivedSolutions =
//       sharedSolutions->syncSolutions({osol}, omp_get_thread_num(), true);
//   for (const auto &sol : receivedSolutions)
//     w.blockDominatedRegion(sol.yPoint);
// }
