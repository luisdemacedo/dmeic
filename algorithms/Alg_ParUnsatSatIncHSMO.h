#ifndef ALG_PARUNSATSATINCHSMO_H
#define ALG_PARUNSATSATINCHSMO_H

#include "Alg_UnsatSatIncHSMO.h"
#include "../clausesharing/DequeSharedClausesBag.h"
#include "../clausesharing/SizeHeuristic.h"
#include <memory>
#include <mutex>

namespace openwbo {

class ParUnsatSatIncHSMO : public UnsatSatIncHSMO {
public:
  ParUnsatSatIncHSMO(int verb, int weight, int strategy, int enc, int pb,
                     int pbobjf, size_t nWorkers, bool clauseSharing,
                     double stride = 1.0, int expansionOrder = 0);
  ~ParUnsatSatIncHSMO() override;

  bool buildWorkFormula() override;
  StatusCode searchAgain() override;
  bool searchUnsatSatMO() override;
  bool rootedSearch(const YPoint &yp) override;
  void checkSols() override;
  void addDiagnosis(const vec<Lit> &clause);

private:
  struct Worker {
    std::unique_ptr<Solver> solver;
    vec<Lit> assumptions;
    size_t nextUpdate = 0;
    int satCalls = 0;
    int satisfiable = 0;
    clausesharing::SizeHeuristic heuristic;
  };

  struct BlockingUpdate {
    int variable;
    std::vector<Lit> clause;
  };

  void prepareSearch();
  void synchronize(size_t wid, const YPoint &fence);
  void shareWorkerClauses(size_t wid);
  void publishModel(size_t wid);
  void exploreFence(size_t wid, const YPoint &fence,
                    waiting_list::WaitingListI &pending);
  std::vector<YPoint> expansionPoints(size_t wid, const YPoint &fence);
  std::vector<Lit> blockingClause(const YPoint &point, int variable);

  std::vector<std::unique_ptr<Worker>> workers;
  std::vector<rootLits_t> slicedRoots;
  std::vector<BlockingUpdate> updates;
  std::set<int> activeBlocks;
  std::mutex stateMutex;
  std::unique_ptr<clausesharing::DequeSharedClausesBag> clausePool;
  bool clauseSharing;
  double stride;
  int expansionOrder;
  size_t sharedVarCutoff = 0;
};

} // namespace openwbo

#endif
