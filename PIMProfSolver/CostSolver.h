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
  public:
    std::vector<COST> thread_elapsed_time;
    double parallelism;
    bool dirty;

    ThreadBBLStats(const BBLStats &bblstats) : BBLStats(bblstats)
    {
        thread_elapsed_time.push_back(bblstats.elapsed_time);
        parallelism = 0;
    }

    ThreadBBLStats& MergeStats(const BBLStats &rhs) {
        BBLStats::MergeStats(rhs);
        thread_elapsed_time.push_back(rhs.elapsed_time);
        return *this;
    }

    void SortElapsedTime() {
        std::sort(thread_elapsed_time.begin(), thread_elapsed_time.end());
    }

    COST MaxElapsedTime() {
        if (dirty) {
            SortElapsedTime();
            elapsed_time = thread_elapsed_time[thread_elapsed_time.size() - 1];
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

    inline const std::vector<ThreadBBLStats *> *getSorted() {
        if (_dirty) {

            SortStatsMap(_bblhash2stats[CPU], _sortedstats[CPU]);
            // align CPU with PIM
            for (auto it = _sortedstats[CPU].begin(); it != _sortedstats[CPU].end(); ++it) {
                UUID bblhash = (*it)->bblhash;
                BBLID bblid = (*it)->bblid;
                auto p = _bblhash2stats[PIM].find(bblhash);
                if (p != _bblhash2stats[PIM].end()) {
                    _sortedstats[PIM].push_back(p->second);
                    p->second->bblid = bblid;
                }
                else {
                    ThreadBBLStats *stats = new ThreadBBLStats(BBLStats(bblid, bblhash));
                    _bblhash2stats[PIM].insert(std::make_pair(bblhash, stats));
                    _sortedstats[PIM].push_back(stats);
                }
            }

            assert(_sortedstats[CPU].size() == _sortedstats[PIM].size());
            for (BBLID i = 0; i < _sortedstats[CPU].size(); i++) {
                assert(_sortedstats[CPU][i]->bblid == _sortedstats[PIM][i]->bblid);
                assert(_sortedstats[CPU][i]->bblhash == _sortedstats[PIM][i]->bblhash);
            }
            _dirty = false;
        }
        return _sortedstats;
    }

    DECISION PrintSolution(std::ostream &out);

    void TrieBFS(COST &cost, const DECISION &decision, BBLID bblid, BBLIDTrieNode *root, bool isDifferent);

    COST Cost(const DECISION &decision, BBLIDTrieNode *reusetree);

    void ReadConfig(ConfigReader &reader);

    std::ostream &PrintDecision(std::ostream &out, const DECISION &decision, bool toscreen);
    // std::ostream &PrintDecisionStat(std::ostream &out, const DECISION &decision, const std::string &name);
    // std::ostream &PrintCostBreakdown(std::ostream &out, const DECISION &decision, const std::string &name);
    // std::ostream &PrintAnalytics(std::ostream &out);

    void PrintStats(std::ostream &ofs)
    {
        const std::vector<ThreadBBLStats *> *sorted = getSorted();

        for (int i = 0; i < MAX_COST_SITE; ++i) {
            ofs << HORIZONTAL_LINE << std::endl;
            ofs << std::setw(7) << "BBLID"
                << std::setw(15) << "Time(ns)"
                << std::setw(15) << "Instruction"
                << std::setw(15) << "Memory Access"
                << std::setw(18) << "Hash(hi)"
                << std::setw(18) << "Hash(lo)"
                << std::endl;
            for (auto it = sorted[i].begin(); it != sorted[i].end(); ++it)
            {
                UUID bblhash = (*it)->bblhash;
                ofs << std::setw(7) << (*it)->bblid
                    << std::setw(15) << (*it)->elapsed_time
                    << std::setw(15) << (*it)->instruction_count
                    << std::setw(15) << (*it)->memory_access
                    << "  " << std::hex
                    << std::setfill('0') << std::setw(16) << bblhash.first
                    << "  "
                    << std::setfill('0') << std::setw(16) << bblhash.second
                    << std::setfill(' ') << std::dec << std::endl;
            }
        }
        
    }

  private:
    DECISION PrintMPKISolution(std::ostream &out);
    DECISION PrintReuseSolution(std::ostream &out);
};

} // namespace PIMProf


#endif // __COSTSOLVER_H__
