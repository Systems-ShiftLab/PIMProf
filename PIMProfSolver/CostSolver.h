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
// as well as the merged RunStats
class ThreadRunStats : public RunStats {
  private:
    std::vector<COST> thread_elapsed_time;
    std::vector<COST> sorted_elapsed_time;
    bool dirty = true;

  public:
    ThreadRunStats(int tid, const RunStats &bblstats) : RunStats(bblstats)
    {
        if (tid >= (int)thread_elapsed_time.size()) {
            thread_elapsed_time.resize(tid + 1, 0);
            sorted_elapsed_time.resize(tid + 1, 0);
        }
        thread_elapsed_time[tid] = bblstats.elapsed_time;
        sorted_elapsed_time[tid] = bblstats.elapsed_time;
    }

    ThreadRunStats& MergeStats(int tid, const RunStats &rhs) {
        if (tid >= (int)thread_elapsed_time.size()) {
            thread_elapsed_time.resize(tid + 1, 0);
            sorted_elapsed_time.resize(tid + 1, 0);
        }
        RunStats::MergeStats(rhs);
        thread_elapsed_time[tid] = rhs.elapsed_time;
        sorted_elapsed_time[tid] = rhs.elapsed_time;
        return *this;
    }

    ThreadRunStats& MergeStats(const ThreadRunStats &rhs) {
        size_t rhssize = rhs.thread_elapsed_time.size();
        if (thread_elapsed_time.size() < rhssize) {
            thread_elapsed_time.resize(rhssize, 0);
            sorted_elapsed_time.resize(rhssize, 0);
        }
        RunStats::MergeStats(rhs);
        for (size_t tid = 0; tid < rhssize; ++tid) {
            thread_elapsed_time[tid] += rhs.thread_elapsed_time[tid];
            dirty = true;
        }
        return *this;
    }

    int parallelism() {
        int result = 0;
        for (COST elem: thread_elapsed_time) {
            if (elem > 0) result++;
        }
        return result;
    }

    void SortElapsedTime() {
        std::sort(sorted_elapsed_time.begin(), sorted_elapsed_time.end());
        dirty = false;
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

inline void SortStatsMap(UUIDHashMap<ThreadRunStats *> &statsmap, std::vector<ThreadRunStats *> &sorted)
{
    sorted.clear();
    for (auto it = statsmap.begin(); it != statsmap.end(); ++it) {
        sorted.push_back(it->second);
    }
    std::sort(
        sorted.begin(),
        sorted.end(),
        [](ThreadRunStats *lhs, ThreadRunStats *rhs) { return lhs->bblhash < rhs->bblhash; });
}

class CostSolver {
  public:
    typedef DataReuse<ThreadRunStats *> FuncDataReuse;
    typedef DataReuseSegment<ThreadRunStats *> FuncDataReuseSegment;
    typedef TrieNode<ThreadRunStats *> FuncTrieNode;

    typedef DataReuse<BBLID> BBLIDDataReuse;
    typedef DataReuseSegment<BBLID> BBLIDDataReuseSegment;
    typedef TrieNode<BBLID> BBLIDTrieNode;

  private:
    CommandLineParser *_command_line_parser;

    // instance of get_id function, prototype:
    // BBLID get_id(Ty elem);
    static BBLID _get_id(BBLID bblid) { return bblid; }
  
  // track function level runstats
  private:
    UUIDHashMap<ThreadRunStats *> _func_hash2stats[MAX_COST_SITE];
    std::vector<ThreadRunStats *> _func_sorted_stats[MAX_COST_SITE];
    FuncDataReuse _func_data_reuse;
    SwitchCountList _func_switch_count;

  // track BBL level runstats
  private:
    UUIDHashMap<ThreadRunStats *> _bbl_hash2stats[MAX_COST_SITE];
    std::vector<ThreadRunStats *> _bbl_sorted_stats[MAX_COST_SITE];
    bool _dirty = true; // track if _bbl_sorted_stats is stale

    BBLIDDataReuse _bbl_data_reuse;
    SwitchCountList _bbl_switch_count;

    /// the cache flush/fetch cost of each site, in nanoseconds
    COST _flush_cost[MAX_COST_SITE];
    COST _fetch_cost[MAX_COST_SITE];

    /// the switch cost FROM each site (TO the other)
    COST _switch_cost[MAX_COST_SITE];

    double _batch_threshold;
    int _batch_size;
    int _mpki_threshold;
    int _parallelism_threshold;

  public:
    void initialize(CommandLineParser *parser);
    ~CostSolver();

    inline COST SingleSegMaxReuseCost() {
        return std::max(
            _flush_cost[CPU] + _fetch_cost[PIM],
            _flush_cost[PIM] + _fetch_cost[CPU]);
    }

    void ParseStats(std::istream &ifs, UUIDHashMap<ThreadRunStats *> &stats);
    void ParseReuse(std::istream &ifs, BBLIDDataReuse &reuse, SwitchCountList &switchcnt);

    // const std::vector<ThreadRunStats *>* getFuncSortedStats();
    const std::vector<ThreadRunStats *>* getBBLSortedStats();

    DECISION PrintSolution(std::ostream &out);


    COST Cost(const DECISION &decision, const BBLIDTrieNode *reusetree, const SwitchCountList &switchcnt);
    COST ElapsedTime(CostSite site); // return CPU/PIM only elapsed time
    std::pair<COST, COST> ElapsedTime(const DECISION &decision); // return execution time pair (cpu_elapsed_time, pim_elapsed_time) for decision
    COST SwitchCost(const DECISION &decision, const SwitchCountList &switchcnt);
    COST ReuseCost(const DECISION &decision, const BBLIDTrieNode *reusetree);
    void TrieBFS(COST &cost, const DECISION &decision, BBLID bblid, const BBLIDTrieNode *root, bool isDifferent);

    void ReadConfig(ConfigReader &reader);

    std::ostream &PrintDecision(std::ostream &out, const DECISION &decision, bool toscreen);
    // std::ostream &PrintDecisionStat(std::ostream &out, const DECISION &decision, const std::string &name);
    // std::ostream &PrintCostBreakdown(std::ostream &out, const DECISION &decision, const std::string &name);
    // std::ostream &PrintAnalytics(std::ostream &out);

    void PrintStats(std::ostream &ofs);

  // BBL level to function level stats converter
  private:
    void BBL2Func(UUIDHashMap<ThreadRunStats *> &bbl, UUIDHashMap<ThreadRunStats *> &func);
    void BBL2Func(BBLIDDataReuse &bbl, FuncDataReuse &func);
    void BBL2Func(SwitchCountList &bbl, SwitchCountList &func);

  private:
    COST PermuteDecision(DECISION &decision, const std::vector<BBLID> &cur_batch, const BBLIDTrieNode *partial_root);

    DECISION PrintMPKIStats(std::ostream &ofs);
    DECISION PrintReuseStats(std::ostream &ofs);
    DECISION PrintGreedyStats(std::ostream &ofs);
    void PrintDisjointSets(std::ostream &ofs);
    DECISION Debug_StartFromUnimportantSegment(std::ostream &ofs);
    DECISION Debug_ConsiderSwitchCost(std::ostream &ofs);
    DECISION Debug_HierarchicalDecision(std::ostream &ofs);
};

} // namespace PIMProf


#endif // __COSTSOLVER_H__
