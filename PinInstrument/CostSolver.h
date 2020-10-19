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
#include <unordered_map>

#include "PinUtil.h"
#include "DataReuse.h"

namespace PIMProf
{
class HashFunc
{
  public:
    // assuming UUID is already murmurhash-ed.
    std::size_t operator()(const UUID &key) const
    {
        size_t result = key.first ^ key.second;
        return result;
    }
};

class BBLStats {
  public:
    COST elapsed_time; // in nanoseconds
    uint64_t instruction_count;
    uint64_t memory_access;
    BBLStats() : elapsed_time(0), instruction_count(0), memory_access(0) {}
    BBLStats& operator += (const BBLStats& rhs) {

        this->elapsed_time += rhs.elapsed_time;
        this->instruction_count += rhs.instruction_count;
        this->memory_access += rhs.memory_access;
        return *this;
    }
};

class BBLStatsMap
{
  public:
    class BBLStatsPair
    {
      public:
        BBLID bblid;
        BBLStats bblstats[MAX_COST_SITE];
        BBLStatsPair() {
            bblid = 0;
            for (int i = 0; i < MAX_COST_SITE; i++) {
                bblstats[i] = BBLStats();
            }
        }
    };

    std::unordered_map<UUID, BBLStatsPair, HashFunc> _bblstats_map;
    std::vector<std::pair<UUID, BBLStatsPair>> _bblstats_sorted;
    bool _bblstats_dirty = true; // dirty flag for _bblstats_sorted

    BBLStatsMap() {
        // set global BBL with UUID(0, 0) as basic block 0
        BBLStats _bblstats[2];
        _bblstats_map[UUID(0, 0)] = BBLStatsPair();
    }

    inline void insert(UUID bblhash, BBLStats bblstats, CostSite site) {
        // iterator points to a BBLStatsPair
        auto it = _bblstats_map.find(bblhash);
        if (it == _bblstats_map.end()) {
            it = _bblstats_map.insert(std::make_pair(bblhash, BBLStatsPair())).first;
            it->second.bblid = _bblstats_map.size() - 1;
        }
        it->second.bblstats[site] += bblstats;
        _bblstats_dirty = true;
    }

    inline const std::unordered_map<UUID, BBLStatsPair, HashFunc> &getMap() {
        return _bblstats_map;
    }

    inline const std::vector<std::pair<UUID, BBLStatsPair>> &getSorted() {
        if (!_bblstats_dirty) {
            return _bblstats_sorted;
        }
        _bblstats_sorted.clear();
        for (auto it = _bblstats_map.begin(); it != _bblstats_map.end(); ++it) {
            _bblstats_sorted.push_back(*it);
        }
        std::sort(_bblstats_sorted.begin(), _bblstats_sorted.end(),
        [](auto &a, auto &b) { return a.second.bblid < b.second.bblid; });

        _bblstats_dirty = false;
        return _bblstats_sorted;
    }

    inline size_t size() { return _bblstats_map.size(); }
};

class CostSolver {
  public:
    /// A DECISION is a vector that represents a certain offloading decision, for example:
    /// A DECISION vector (PIM, CPU, CPU, PIM) means:
    /// put the 1st and 4th BBL on PIM and 2nd and 3rd on CPU for execution
    /// The target of CostSolver is to figure out the decision that will lead to the minimum total cost.
    typedef std::vector<CostSite> DECISION;

  private:
    CommandLineParser *_command_line_parser;
    BBLStatsMap _bblstats_map;

    // std::vector<COST> _BBL_partial_total[MAX_COST_SITE];
    DataReuse _data_reuse;
// #ifdef PIMPROF_MPKI
//     std::vector<COST> _BBL_storage_partial_total[MAX_COST_SITE][MAX_LEVEL];
// #endif

    /// the cache flush/fetch cost of each site, in nanoseconds
    COST _flush_cost[MAX_COST_SITE];
    COST _fetch_cost[MAX_COST_SITE];

    double _batchthreshold;
    int _batchsize;
    int _mpkithreshold;
 

  public:
    void initialize(CommandLineParser *parser);

    DECISION PrintSolution(std::ostream &out);

    void TrieBFS(COST &cost, const DECISION &decision, BBLID bblid, TrieNode *root, bool isDifferent);

    COST Cost(const DECISION &decision, TrieNode *reusetree);

    void ReadConfig(ConfigReader &reader);

    std::ostream &PrintDecision(std::ostream &out, const DECISION &decision, bool toscreen);
    // std::ostream &PrintDecisionStat(std::ostream &out, const DECISION &decision, const std::string &name);
    // std::ostream &PrintCostBreakdown(std::ostream &out, const DECISION &decision, const std::string &name);
    // std::ostream &PrintAnalytics(std::ostream &out);

  private:
    DECISION PrintMPKISolution(std::ostream &out);
    DECISION PrintPIMProfSolution(std::ostream &out);
};

} // namespace PIMProf


#endif // __COSTSOLVER_H__
