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
// provide separate elapsed time for each thread
// as well as the merged BBLStats
class ThreadBBLStats : public BBLStats {
  private:
    std::vector<COST> thread_elapsed_time;
    std::vector<COST> sorted_elapsed_time;
    int parallelism;
    bool dirty = true;

  public:
    ThreadBBLStats(int tid, const BBLStats &bblstats) : BBLStats(bblstats)
    {
        if (tid >= (int)thread_elapsed_time.size()) {
            thread_elapsed_time.resize(tid + 1, 0);
            sorted_elapsed_time.resize(tid + 1, 0);
        }
        thread_elapsed_time[tid] = bblstats.elapsed_time;
        sorted_elapsed_time[tid] = bblstats.elapsed_time;
        parallelism = 0;
    }

    ThreadBBLStats& MergeStats(int tid, const BBLStats &rhs) {
        if (tid >= (int)thread_elapsed_time.size()) {
            thread_elapsed_time.resize(tid + 1, 0);
            sorted_elapsed_time.resize(tid + 1, 0);
        }
        BBLStats::MergeStats(rhs);
        thread_elapsed_time[tid] = rhs.elapsed_time;
        sorted_elapsed_time[tid] = rhs.elapsed_time;
        parallelism++;
        return *this;
    }

    void SortElapsedTime() {
        std::sort(sorted_elapsed_time.begin(), sorted_elapsed_time.end());
    }

    COST ElapsedTime(int tid) {
        assert(tid < (int)thread_elapsed_time.size());
        return thread_elapsed_time[tid];
    }

    COST MaxElapsedTime() {
        if (dirty) {
            SortElapsedTime();
            elapsed_time = sorted_elapsed_time[thread_elapsed_time.size() - 1];
        }
        return elapsed_time;
    }

    void print(std::ostream &ofs) {
        ofs << bblid << ","
            << std::hex << bblhash.first << "," << bblhash.second << "," << std::dec;
        for (auto elem : thread_elapsed_time) {
            ofs << elem << ",";
        }
        ofs << std::endl;
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
    typedef DataReuse<BBLID> BBLIDDataReuse;
    typedef DataReuseSegment<BBLID> BBLIDDataReuseSegment;
    typedef TrieNode<BBLID> BBLIDTrieNode;
    typedef BBLSwitchCountList<BBLID> BBLIDBBLSwitchCountList;

    // instance of get_id function, prototype:
    // BBLID get_id(Ty elem);
    static BBLID _get_id(BBLID bblid) { return bblid; }

  private:
    CommandLineParser *_command_line_parser;
    UUIDHashMap<ThreadBBLStats *> _bblhash2stats[MAX_COST_SITE];
    std::vector<ThreadBBLStats *> _sortedstats[MAX_COST_SITE];
    bool _dirty = true; // track if _sortedstats is stale

    BBLIDDataReuse _data_reuse;
    BBLIDBBLSwitchCountList _bbl_switch_count;

    /// the cache flush/fetch cost of each site, in nanoseconds
    COST _flush_cost[MAX_COST_SITE];
    COST _fetch_cost[MAX_COST_SITE];

    /// the switch cost FROM each site (TO the other)
    COST _switch_cost[MAX_COST_SITE];

    double _batch_threshold;
    int _batch_size;
    int _mpki_threshold;

  public:
    void initialize(CommandLineParser *parser);
    ~CostSolver();

    inline COST SingleSegMaxReuseCost() {
        return std::max(
            _flush_cost[CPU] + _fetch_cost[PIM],
            _flush_cost[PIM] + _fetch_cost[CPU]);
    }

    void ParseStats(std::istream &ifs, UUIDHashMap<ThreadBBLStats *> &stats);
    void ParseReuse(std::istream &ifs, BBLIDDataReuse &reuse);

    const std::vector<ThreadBBLStats *>* getSorted();

    DECISION PrintSolution(std::ostream &out);


    COST Cost(const DECISION &decision, const BBLIDTrieNode *reusetree, const BBLIDBBLSwitchCountList &switchcnt);
    COST ElapsedTime(CostSite site); // return CPU/PIM only elapsed time
    std::pair<COST, COST> ElapsedTime(const DECISION &decision); // return execution time pair (cpu_elapsed_time, pim_elapsed_time) for decision
    COST SwitchCost(const DECISION &decision, const BBLIDBBLSwitchCountList &switchcnt);
    COST ReuseCost(const DECISION &decision, const BBLIDTrieNode *reusetree);
    void TrieBFS(COST &cost, const DECISION &decision, BBLID bblid, const BBLIDTrieNode *root, bool isDifferent);

    void ReadConfig(ConfigReader &reader);

    std::ostream &PrintDecision(std::ostream &out, const DECISION &decision, bool toscreen);
    // std::ostream &PrintDecisionStat(std::ostream &out, const DECISION &decision, const std::string &name);
    // std::ostream &PrintCostBreakdown(std::ostream &out, const DECISION &decision, const std::string &name);
    // std::ostream &PrintAnalytics(std::ostream &out);

    void PrintStats(std::ostream &ofs);

  private:
    COST PermuteDecision(DECISION &decision, const std::vector<BBLID> &cur_batch, const BBLIDTrieNode *partial_root);

    DECISION PrintMPKIStats(std::ostream &ofs);
    DECISION PrintReuseStats(std::ostream &ofs);
    DECISION PrintGreedyStats(std::ostream &ofs);
    void PrintDisjointSets(std::ostream &ofs);
    DECISION Debug_StartFromUnimportantSegment(std::ostream &ofs);
    DECISION Debug_ConsiderSwitchCost(std::ostream &ofs);
};

} // namespace PIMProf


#endif // __COSTSOLVER_H__
