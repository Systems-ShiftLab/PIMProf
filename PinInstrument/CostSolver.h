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
#include "pin.H"

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

    COST _clwb_cost;
    COST _invalidate_cost;
    COST _fetch_cost;
    int _batchcount;
    int _batchsize;

  public:
    void initialize(CostPackage *cost_package, ConfigReader &reader);

    DECISION PrintSolution(std::ostream &out);

    VOID TrieBFS(COST &cost, const DECISION &decision, BBLID bblid, TrieNode *root, bool isDifferent);

    COST Cost(const DECISION &decision, TrieNode *reusetree);

    DECISION FindOptimal();

    VOID ReadConfig(ConfigReader &reader);

    VOID SetBBLSize(BBLID bbl_size);

    std::ostream &PrintDecision(std::ostream &out, const DECISION &decision, bool toscreen);
    std::ostream &PrintDecisionStat(std::ostream &out, const DECISION &decision, const std::string &name);
    std::ostream &PrintBBLDecisionStat(std::ostream &out, const DECISION &decision, bool toscreen);
};

} // namespace PIMProf


#endif // __COSTSOLVER_H__