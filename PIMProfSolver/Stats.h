//===- Stats.h - Header-only class for stats collection ---------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#ifndef __STATS_H__
#define __STATS_H__

#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <unistd.h>
#include <cmath>
#include <list>
#include <set>
#include <map>
#include <algorithm>
#include <unordered_map>
#include <cassert>

#include "Common.h"
#include "Util.h"
#include "DataReuse.h"

namespace PIMProf
{
/* ===================================================================== */
/* PIMProf Thread Data Collection */
/* ===================================================================== */

class RunStats
{
public:
    // For multithread stats, all threads should achieve a consensus on bblid.
    // Therefore, the bblid field should be filled after all threads finish execution
    BBLID bblid;
    UUID bblhash;
    COST elapsed_time; // store the nanosecond count of each basic block
    uint64_t instruction_count;
    uint64_t memory_access;

    // instance of get_id function, prototype:
    // BBLID get_id(Ty elem);
    static BBLID _get_id(RunStats *stats) { return stats->bblid; }

    RunStats(
        BBLID _bblid = GLOBAL_BBLID,
        UUID _bblhash = GLOBAL_BBLHASH,
        COST _elapsed_time = 0,
        uint64_t _instruction_count = 0,
        uint64_t _memory_access = 0)
        : bblid(_bblid), bblhash(_bblhash), elapsed_time(_elapsed_time), instruction_count(_instruction_count), memory_access(_memory_access)
    {
    }

    // RunStats(const RunStats &rhs) {
    //     bblid = rhs.bblid;
    //     bblhash = rhs.bblhash;
    //     elapsed_time = rhs.elapsed_time;
    //     instruction_count = rhs.instruction_count;
    //     memory_access = rhs.memory_access;
    // }

    RunStats& MergeStats(const RunStats &rhs) {
        elapsed_time += rhs.elapsed_time;
        instruction_count += rhs.instruction_count;
        memory_access += rhs.memory_access;
        return *this;
    }

    RunStats& operator += (const RunStats& rhs) {
        MergeStats(rhs);
        return *this;
    }
};

// the pointers in sorted will point to the same location as the pointers in statsmap
inline void SortStatsMap(UUIDHashMap<RunStats *> &statsmap, std::vector<RunStats *> &sorted)
{
    sorted.clear();
    for (auto it = statsmap.begin(); it != statsmap.end(); ++it) {
        sorted.push_back(it->second);
    }
    std::sort(
        sorted.begin(),
        sorted.end(),
        [](RunStats *lhs, RunStats *rhs) { return lhs->bblhash < rhs->bblhash; });
}

class ThreadStats
{
    typedef SwitchCountMatrix<RunStats *> PtrSwitchCountMatrix;
    typedef DataReuseSegment<RunStats *> PtrDataReuseSegment;
    typedef DataReuse<RunStats *> PtrDataReuse;

private:
    int tid;
    COST m_pim_time;

    int m_switch_cpu2pim;
    int m_switch_pim2cpu;

    std::vector<bool> *m_using_pim;
    std::vector<RunStats *> *m_current_bblstats;

    // UUID is the unique identifier of each BBL,
    // and there is a one-to-one correspondence between UUID and RunStats*.
    // All class objects need to be stored in pointer form,
    // otherwise Sniper will somehow deallocate them unexpectedly.
    UUIDHashMap<RunStats *> *m_bbl_hash2stats;

    // count the number of times BBL switch from one to another
    PtrSwitchCountMatrix *m_bbl_switch_count;

    // a map from tag to data reuse segments
    std::unordered_map<uint64_t, PtrDataReuseSegment *> *m_tag2seg;

    // data structure for storing data reuse info
    PtrDataReuse *m_bbl_data_reuse;

public:
    ThreadStats(int _tid = 0)
        : tid(_tid)
        , m_pim_time(0)
        , m_switch_cpu2pim(0)
        , m_switch_pim2cpu(0)
    {
        m_using_pim = new std::vector<bool>;
        m_using_pim->push_back(false);

        // GLOBAL_BBLHASH is the region outside main function.
        m_bbl_hash2stats = new UUIDHashMap<RunStats *>;
        RunStats *globalstats = new RunStats();
        m_bbl_hash2stats->insert(std::make_pair(GLOBAL_BBLHASH, globalstats));

        m_current_bblstats = new std::vector<PIMProf::RunStats *>;
        m_current_bblstats->push_back(globalstats);

        m_bbl_switch_count = new PtrSwitchCountMatrix();
        m_tag2seg = new std::unordered_map<uint64_t, PtrDataReuseSegment *>;
        m_bbl_data_reuse = new PtrDataReuse();
    }

    ~ThreadStats()
    {
        COST total = 0;
        std::vector<std::pair<UUID, COST>> sorted(m_bblhash2cputime.begin(), m_bblhash2cputime.end());
        std::sort(sorted.begin(), sorted.end(),
            [](const std::pair<UUID, COST> &lhs, const std::pair<UUID, COST> &rhs) {
                return lhs.first.first < rhs.first.first;
            }
        );
        for(auto it : sorted) {
            total += it.second;
            std::cout << std::hex << it.first.first << " " << it.first.second << std::dec << " " << it.second << std::endl;
        }
        std::cout << "tid = " << tid << ", total = " << total << std::endl;

        std::cout << "switch CPU to PIM = " << m_switch_cpu2pim << std::endl;
        std::cout << "switch PIM to CPU = " << m_switch_pim2cpu << std::endl;

        delete m_using_pim;
        delete m_current_bblstats;

        for (auto it = m_bbl_hash2stats->begin(); it != m_bbl_hash2stats->end(); ++it)
        {
            delete it->second;
        }
        delete m_bbl_hash2stats;

        delete m_bbl_switch_count;

        for (auto it = m_tag2seg->begin(); it != m_tag2seg->end(); ++it)
        {
            delete it->second;
        }
        delete m_tag2seg;

        delete m_bbl_data_reuse;
    }

    void setTid(int _tid) { tid = _tid; }
    bool IsUsingPIM() { return m_using_pim->back(); }

    RunStats *GetCurrentRunStats() { return m_current_bblstats->back(); }
    UUID GetCurrentBBLHash() { return m_current_bblstats->back()->bblhash; }

    void *stats_addr = NULL;
    size_t stats_size = 0;

    void BBLStart(uint64_t hi, uint64_t lo)
    {
        UUID bblhash = UUID(hi, lo);
        RunStats *prev_back = GetCurrentRunStats();
        auto it = m_bbl_hash2stats->find(bblhash);
        
        
        if (it == m_bbl_hash2stats->end()) {
            RunStats *stats = new RunStats(GLOBAL_BBLID, bblhash);
            m_bbl_hash2stats->insert(std::make_pair(bblhash, stats));
            m_current_bblstats->push_back(stats);
        }
        else {
            m_current_bblstats->push_back(it->second);
        }
        m_bbl_switch_count->insert(prev_back, GetCurrentRunStats());
    }

    void BBLEnd(uint64_t hi, uint64_t lo)
    {
        UUID bblhash = UUID(hi, lo);
        RunStats *prev_back = GetCurrentRunStats();
        // printf("%d: %lx %lx %lx %lx\n", tid, bblhash.first, bblhash.second, GetCurrentBBLHash().first, GetCurrentBBLHash().second);
        if (bblhash != GetCurrentBBLHash()) 
        {
            printf("BBLEnd annotator not match: %d %lx %lx %lx %lx\n", tid, bblhash.first, bblhash.second, GetCurrentBBLHash().first, GetCurrentBBLHash().second);
        }
        assert(bblhash == GetCurrentBBLHash());
        m_current_bblstats->pop_back();
        m_bbl_switch_count->insert(prev_back, GetCurrentRunStats());
    }

    void OffloadStart(uint64_t hi, uint64_t type)
    {
        // printf("Start %d %lx %lu\n", tid, hi, type);
        bool prev_back = m_using_pim->back();
        m_using_pim->push_back(type == PIMPROF_DECISION_PIM);

        if (prev_back != m_using_pim->back()) {
            if (prev_back) {
                m_switch_pim2cpu++;
            }
            else {
                m_switch_cpu2pim++;
            }
        }
        // our compiling tool uses only the high bits of bblhash
        // to distinguish BBLs in this case
        BBLStart(hi, 0);
    }

    void OffloadEnd(uint64_t hi, uint64_t type)
    {
        // printf("End %d %lx %lu\n", tid, hi, type);
        if (m_using_pim->back() != (type == PIMPROF_DECISION_PIM)) {
            printf("OffloadEnd annotator not match: %d %lx %lu\n", tid, hi, type);
        }
        assert(m_using_pim->back() == (type == PIMPROF_DECISION_PIM));
        bool prev_back = m_using_pim->back();
        m_using_pim->pop_back();
        if (prev_back != m_using_pim->back()) {
            if (prev_back) {
                m_switch_pim2cpu++;
            }
            else {
                m_switch_cpu2pim++;
            }
        }
        BBLEnd(hi, 0);
    }

    // time unit is FS (1e-6 NS)
    void AddTimeInstruction(uint64_t time, uint64_t instr)
    {
        RunStats *bblstats = GetCurrentRunStats();
        bblstats->elapsed_time += (COST)time / 1e6;
        bblstats->instruction_count += instr;
    }

    void AddMemory(uint64_t memory_access)
    {
        GetCurrentRunStats()->memory_access += memory_access;
    }

    // time unit is FS (1e-6 NS)
    void AddOffloadingTime(uint64_t time)
    {
        m_pim_time += (COST)time / 1e6;
    }

    void InsertSegOnHit(uintptr_t tag, bool is_store)
    {
        PtrDataReuseSegment *seg;
        auto it = m_tag2seg->find(tag);
        if (it == m_tag2seg->end())
        {
            seg = new PtrDataReuseSegment();
            m_tag2seg->insert(std::make_pair(tag, seg));
        }
        else
        {
            seg = it->second;
        }
        RunStats *bblstats = GetCurrentRunStats();
        seg->insert(bblstats);
        // int32_t threadcount = _storage->_cost_package->_thread_count;
        // if (threadcount > seg->getCount())
        seg->setCount(1);
        // split then insert on store
        if (is_store)
        {
            m_bbl_data_reuse->UpdateTrie(m_bbl_data_reuse->getRoot(), seg);
            seg->clear();
            seg->insert(bblstats);
            // if (threadcount > seg->getCount())
            seg->setCount(1);
        }
    }

    void SplitSegOnMiss(uintptr_t tag)
    {
        PtrDataReuseSegment *seg;
        auto it = m_tag2seg->find(tag);
        if (it == m_tag2seg->end())
            return; // ignore it if there is no existing segment
        seg = it->second;
        m_bbl_data_reuse->UpdateTrie(m_bbl_data_reuse->getRoot(), seg);
        seg->clear();
    }

    void MergeStatsMap(UUIDHashMap<RunStats *> &statsmap)
    {
        for (auto it = m_bbl_hash2stats->begin(); it != m_bbl_hash2stats->end(); ++it) {
            auto p = statsmap.find(it->first);
            if (p == statsmap.end()) {
                RunStats *stats = new RunStats(GLOBAL_BBLID, it->first);
                stats->MergeStats(*it->second);
                statsmap.insert(std::make_pair(it->first, stats));
            }
            else {
                // merge the stats
                p->second->MergeStats(*it->second);
            }
        }
    }

    // threads should achieve consensus on BBLID
    void GenerateBBLID(UUIDHashMap<RunStats *> &statsmap)
    {
        std::vector<RunStats *> sorted;
        SortStatsMap(statsmap, sorted);
        for (BBLID i = 0; i < (int)sorted.size(); ++i) {
            statsmap[sorted[i]->bblhash]->bblid = i;
        }
    }

    void AssignBBLID(UUIDHashMap<RunStats *> &statsmap)
    {

        for (auto it = m_bbl_hash2stats->begin(); it != m_bbl_hash2stats->end(); ++it) {
            UUID bblhash = it->second->bblhash;
            auto p = statsmap.find(bblhash);
            assert(p != statsmap.end());
            it->second->bblid = p->second->bblid;
        }
    }

    void PrintPIMTime(std::ostream &ofs)
    {
        ofs << m_pim_time << std::endl;
    }

    // void PrintDataReuseDotGraph(std::ostream &ofs)
    // {
    //     m_bbl_data_reuse->PrintDotGraph(ofs);
    // }

    void PrintStats(std::ostream &ofs)
    {
        std::vector<RunStats *> sorted;
        SortStatsMap(*m_bbl_hash2stats, sorted);

        ofs << HORIZONTAL_LINE << std::endl;
        ofs << "Thread " << tid << std::endl;
        ofs << std::setw(7) << "BBLID"
            << std::setw(15) << "Time(ns)"
            << std::setw(15) << "Instruction"
            << std::setw(15) << "Memory Access"
            << std::setw(18) << "Hash(hi)"
            << std::setw(18) << "Hash(lo)"
            << std::endl;
        for (auto it : sorted)
        {
            UUID bblhash = it->bblhash;
            ofs << std::setw(7) << it->bblid
                << std::setw(15) << it->elapsed_time
                << std::setw(15) << it->instruction_count
                << std::setw(15) << it->memory_access
                << "  " << std::hex
                << std::setfill('0') << std::setw(16) << bblhash.first
                << "  "
                << std::setfill('0') << std::setw(16) << bblhash.second
                << std::setfill(' ') << std::dec << std::endl;
        }
    }

    void PrintDataReuseSegments(std::ostream &ofs)
    {
        ofs << HORIZONTAL_LINE << std::endl;
        ofs << "ReuseSegment - Thread " << tid << std::endl;
        m_bbl_data_reuse->PrintAllSegments(ofs, RunStats::_get_id);
    }

    void PrintBBLSwitchCount(std::ostream &ofs)
    {
        ofs << HORIZONTAL_LINE << std::endl;
        ofs << "BBLSwitchCount - Thread " << tid << std::endl;
        m_bbl_switch_count->print(ofs, RunStats::_get_id);
    }

  private:
    UUIDHashMap<COST> m_bblhash2cputime;

  public:
    void AddCPUTime(uint64_t time)
    {
        UUID bblhash = GetCurrentBBLHash();
        m_bblhash2cputime[bblhash] += (COST)time / 1e6;
    }
};

} // namespace PIMProf

#endif // __STATS_H__