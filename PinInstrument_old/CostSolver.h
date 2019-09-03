#ifndef __COSTSOLVER_H__
#define __COSTSOLVER_H__

#include <stack>
#include <list>
#include <set>
#include "pin.H"

#include "PinUtil.h"
#include "MemoryLatency.h"
#include "InstructionLatency.h"
#include "DataReuse.h"

namespace PIMProf
{

class CostSolver {
    friend class InstructionLatency;
    friend class MemoryLatency;
    friend class DataReuse;
    friend class PinInstrument;
    friend class CACHE_LEVEL;

  public:
    /// A DECISION is a vector that represents a certain offloading decision, for example:
    /// A DECISION vector (PIM, CPU, CPU, PIM) means:
    /// put the 1st and 4th BBL on PIM and 2nd and 3rd on CPU for execution
    /// The target of CostSolver is to figure out the decision that will lead to the minimum total cost.
    typedef std::vector<CostSite> DECISION;

  private:
    static COST _control_latency[MAX_COST_SITE][MAX_COST_SITE];
    static std::vector<COST> _BBL_instruction_cost[MAX_COST_SITE];
    static std::vector<COST> _BBL_memory_cost[MAX_COST_SITE];
    static COST _instruction_multiplier[MAX_COST_SITE];
    static COST _memory_cost[MAX_COST_SITE];
    static std::vector<COST> _BBL_partial_total[MAX_COST_SITE];

    static COST _clwb_cost;
    static COST _invalidate_cost;
    static COST _fetch_cost;
    static int _batchcount;
    static int _batchsize;

  private:
    static BBLID _BBL_size;

  public:
    CostSolver();
    CostSolver(const std::string filename);
    // static inline VOID clear() { _cost_term_set.clear(); }

    static DECISION PrintSolution(std::ostream &out);

    static VOID TrieBFS(COST &cost, const DECISION &decision, BBLID bblid, TrieNode *root, bool isDifferent);

    static COST Cost(const DECISION &decision, TrieNode *reusetree);

    static DECISION FindOptimal(TrieNode *reusetree);

    static VOID ReadConfig(const std::string filename);

    // static VOID AddCostTerm(const CostTerm &cost);

    static VOID SetBBLSize(BBLID _BBL_size);

    static VOID ReadControlFlowGraph(const std::string filename);

    static std::ostream &PrintDecision(std::ostream &out, const DECISION &decision, bool toscreen);
    static std::ostream &PrintDecisionStat(std::ostream &out, const DECISION &decision, const std::string &name);
    static std::ostream &PrintBBLDecisionStat(std::ostream &out, const DECISION &decision, bool toscreen);
};

} // namespace PIMProf


#endif // __COSTSOLVER_H__