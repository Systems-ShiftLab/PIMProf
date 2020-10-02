//===- CostSolver.h - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#ifndef __COSTSOLVER_H__
#define __COSTSOLVER_H__

#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <unistd.h>
#include <cmath>
#include <stack>
#include <list>
#include <set>
#include <algorithm>

#include "PinUtil.h"
#include "CostPackage.h"

namespace PIMProf
{

class CostSolver {
    friend class PinInstrument;
  public:
    /// A DECISION is a vector that represents a certain offloading decision, for example:
    /// A DECISION vector (PIM, CPU, CPU, PIM) means:
    /// put the 1st and 4th BBL on PIM and 2nd and 3rd on CPU for execution
    /// The target of CostSolver is to figure out the decision that will lead to the minimum total cost.
    typedef std::vector<CostSite> DECISION;

  private:
    CostPackage *_cost_package;

    std::vector<COST> _BBL_partial_total[MAX_COST_SITE];
#ifdef PIMPROF_MPKI
    std::vector<COST> _BBL_storage_partial_total[MAX_COST_SITE][MAX_LEVEL];
#endif

    double _batchthreshold;
    int _batchsize;
    int _mpkithreshold;
  
    /// the cache flush/fetch cost of each site
    COST _flush_cost[MAX_COST_SITE];
    COST _fetch_cost[MAX_COST_SITE];

  public:
    void initialize(CostPackage *cost_package, ConfigReader &reader);

    DECISION PrintSolution(std::ostream &out);

    void TrieBFS(COST &cost, const DECISION &decision, BBLID bblid, TrieNode *root, bool isDifferent);

    COST Cost(const DECISION &decision, TrieNode *reusetree);

    DECISION FindOptimal();

    void ReadConfig(ConfigReader &reader);

    void SetBBLSize(BBLID bbl_size);

    std::ostream &PrintDecision(std::ostream &out, const DECISION &decision, bool toscreen);
    std::ostream &PrintDecisionStat(std::ostream &out, const DECISION &decision, const std::string &name);
    std::ostream &PrintCostBreakdown(std::ostream &out, const DECISION &decision, const std::string &name);
    std::ostream &PrintAnalytics(std::ostream &out);
};

} // namespace PIMProf


#endif // __COSTSOLVER_H__
