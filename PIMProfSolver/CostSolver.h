//===- CostSolver.h - Utils for instrumentation -----------------*- C++ -*-===//
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

#include "Common.h"
#include "Util.h"
#include "Stats.h"

namespace PIMProf
{
class ThreadBBLStats : public BBLStats {
  private:
    std::vector<COST> thread_elapsed_time;
    std::vector<COST> sorted_elapsed_time;
    double parallelism;
    bool dirty = true;

  public:
    ThreadBBLStats(const BBLStats &bblstats) : BBLStats(bblstats)
    {
        thread_elapsed_time.push_back(bblstats.elapsed_time);
        sorted_elapsed_time.push_back(bblstats.elapsed_time);
        parallelism = 0;
    }

    ThreadBBLStats& MergeStats(const BBLStats &rhs) {
        BBLStats::MergeStats(rhs);
        thread_elapsed_time.push_back(rhs.elapsed_time);
        sorted_elapsed_time.push_back(rhs.elapsed_time);
        return *this;
    }

    void SortElapsedTime() {
        std::sort(sorted_elapsed_time.begin(), sorted_elapsed_time.end());
    }

    COST ElapsedTime(int tid) {
        return thread_elapsed_time[tid];
    }

    COST MaxElapsedTime() {
        if (dirty) {
            SortElapsedTime();
            elapsed_time = sorted_elapsed_time[thread_elapsed_time.size() - 1];
        }
        return elapsed_time;
    }

};

inline void SortStatsMap(UUIDHashMap<ThreadBBLStats *> &statsmap, std::vector<ThreadBBLStats *> &sorted)
{
    sorted.clear();
    for (auto it = statsmap.begin(); it != statsmap.end(); ++it) {
        sorted.push_back(it->second);
    }
    std::sort(
        sorted.begin(),
        sorted.end(),
        [](ThreadBBLStats *lhs, ThreadBBLStats *rhs) { return lhs->bblhash < rhs->bblhash; });
}

class CostSolver {
  public:
    /// A DECISION is a vector that represents a certain offloading decision, for example:
    /// A DECISION vector (PIM, CPU, CPU, PIM) means:
    /// put the 1st and 4th BBL on PIM and 2nd and 3rd on CPU for execution
    /// The target of CostSolver is to figure out the decision that will lead to the minimum total cost.
    typedef std::vector<CostSite> DECISION;
    typedef DataReuse<BBLID> BBLIDDataReuse;
    typedef DataReuseSegment<BBLID> BBLIDDataReuseSegment;
    typedef TrieNode<BBLID> BBLIDTrieNode;

  private:
    CommandLineParser *_command_line_parser;
    UUIDHashMap<ThreadBBLStats *> _bblhash2stats[MAX_COST_SITE];
    std::vector<ThreadBBLStats *> _sortedstats[MAX_COST_SITE];
    bool _dirty = true; // track if _sortedstats is stale

    BBLIDDataReuse _data_reuse;

    /// the cache flush/fetch cost of each site, in nanoseconds
    COST _flush_cost[MAX_COST_SITE];
    COST _fetch_cost[MAX_COST_SITE];

    double _batchthreshold;
    int _batchsize;
    int _mpkithreshold;

  public:
    void initialize(CommandLineParser *parser);
    ~CostSolver();

    void ParseStats(std::istream &ifs, UUIDHashMap<ThreadBBLStats *> &stats);
    void ParseReuse(std::istream &ifs, BBLIDDataReuse &reuse);

    const std::vector<ThreadBBLStats *>* getSorted();

    DECISION PrintSolution(std::ostream &out);

    void TrieBFS(COST &cost, const DECISION &decision, BBLID bblid, BBLIDTrieNode *root, bool isDifferent);

    COST Cost(const DECISION &decision, BBLIDTrieNode *reusetree);
    COST ElapsedTime(CostSite site); // return CPU/PIM only elapsd time
    std::pair<COST, COST> ElapsedTime(const DECISION &decision); // return execution time pair (cpu_elapsed_time, pim_elapsed_time) for decision
    COST ReuseCost(const DECISION &decision, BBLIDTrieNode *reusetree);

    void ReadConfig(ConfigReader &reader);

    std::ostream &PrintDecision(std::ostream &out, const DECISION &decision, bool toscreen);
    // std::ostream &PrintDecisionStat(std::ostream &out, const DECISION &decision, const std::string &name);
    // std::ostream &PrintCostBreakdown(std::ostream &out, const DECISION &decision, const std::string &name);
    // std::ostream &PrintAnalytics(std::ostream &out);

    void PrintStats(std::ostream &ofs);

  private:
    DECISION PrintMPKIStats(std::ostream &ofs);
    DECISION PrintReuseStats(std::ostream &ofs);
    DECISION PrintGreedyStats(std::ostream &ofs);
};

} // namespace PIMProf


#endif // __COSTSOLVER_H__
