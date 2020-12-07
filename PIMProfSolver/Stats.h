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

class BBLStats
{
public:
    // For multithread stats, all threads should achieve a consensus on bblid.
    // Therefore, the bblid field should be filled after all threads finish execution
    BBLID bblid;
    UUID bblhash;
    COST elapsed_time; // store the nanosecond count of each basic block
    uint64_t instruction_count;
    uint64_t memory_access;

    BBLStats(
        BBLID _bblid = GLOBAL_BBLID,
        UUID _bblhash = GLOBAL_BBLHASH,
        COST _elapsed_time = 0,
        uint64_t _instruction_count = 0,
        uint64_t _memory_access = 0)
        : bblid(_bblid), bblhash(_bblhash), elapsed_time(_elapsed_time), instruction_count(_instruction_count), memory_access(_memory_access)
    {
    }

    // BBLStats(const BBLStats &rhs) {
    //     bblid = rhs.bblid;
    //     bblhash = rhs.bblhash;
    //     elapsed_time = rhs.elapsed_time;
    //     instruction_count = rhs.instruction_count;
    //     memory_access = rhs.memory_access;
    // }

    BBLStats& MergeStats(const BBLStats &rhs) {
        elapsed_time += rhs.elapsed_time;
        instruction_count += rhs.instruction_count;
        memory_access += rhs.memory_access;
        return *this;
    }

    BBLStats& operator += (const BBLStats& rhs) {
        MergeStats(rhs);
        return *this;
    }
};

typedef DataReuseSegment<BBLStats *> PtrDataReuseSegment;
typedef DataReuse<BBLStats *> PtrDataReuse;

// the pointers in sorted will point to the same location as the pointers in statsmap
inline void SortStatsMap(UUIDHashMap<BBLStats *> &statsmap, std::vector<BBLStats *> &sorted)
{
    sorted.clear();
    for (auto it = statsmap.begin(); it != statsmap.end(); ++it) {
        sorted.push_back(it->second);
    }
    std::sort(
        sorted.begin(),
        sorted.end(),
        [](BBLStats *lhs, BBLStats *rhs) { return lhs->bblhash < rhs->bblhash; });
}

class ThreadStats
{
private:
    int tid;
    COST m_pim_time;

    int m_switch_cpu2pim;
    int m_switch_pim2cpu;

    std::vector<bool> *m_using_pim;
    std::vector<UUID> *m_current_bblhash;

    // all class objects need to be stored in pointer form,
    // otherwise Sniper will somehow deallocate them unexpectedly.
    UUIDHashMap<BBLStats *> *m_bblhash2stats;

    // a map from tag to data reuse segments
    std::unordered_map<uint64_t, PtrDataReuseSegment *> *m_tag2seg;

    // data structure for storing data reuse info
    PtrDataReuse *m_data_reuse;

public:
    ThreadStats(int _tid = 0)
        : tid(_tid)
        , m_pim_time(0)
        , m_switch_cpu2pim(0)
        , m_switch_pim2cpu(0)
    {
        m_using_pim = new std::vector<bool>;
        m_using_pim->push_back(false);

        m_current_bblhash = new std::vector<UUID>;
        m_current_bblhash->push_back(GLOBAL_BBLHASH);

        // GLOBAL_BBLHASH is the region outside main function.
        m_bblhash2stats = new UUIDHashMap<BBLStats *>;
        m_bblhash2stats->insert(std::make_pair(GLOBAL_BBLHASH, new BBLStats()));

        m_tag2seg = new std::unordered_map<uint64_t, PtrDataReuseSegment *>;

        m_data_reuse = new PtrDataReuse();
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
        for (auto it : sorted) {
            total += it.second;
            std::cout << std::hex << it.first.first << " " << it.first.second << std::dec << " " << it.second << std::endl;
        }
        std::cout << "TOTAL = " << total << std::endl;

        std::cout << "Switch CPU to PIM = " << m_switch_cpu2pim << std::endl;
        std::cout << "Switch PIM to CPU = " << m_switch_pim2cpu << std::endl;

        delete m_using_pim;
        delete m_current_bblhash;

        for (auto it = m_bblhash2stats->begin(); it != m_bblhash2stats->end(); ++it)
        {
            delete it->second;
        }
        delete m_bblhash2stats;

        for (auto it = m_tag2seg->begin(); it != m_tag2seg->end(); ++it)
        {
            delete it->second;
        }
        delete m_tag2seg;

        delete m_data_reuse;
    }

    void setTid(int _tid) { tid = _tid; }
    bool IsUsingPIM() { return m_using_pim->back(); }

    BBLStats *GetBBLStats(UUID temp)
    {
        
        // auto it0 = m_bblhash2stats->find(bblhash);
        // auto it = m_bblhash2stats->find(bblhash);
        // auto it2 = m_bblhash2stats->find(bblhash);
        // if (bblhash == UUID(0, 0))
        //     printf("tid=%d size=%lu hash=%lx %lx\n", tid, m_bblhash2stats->size(), bblhash.first, bblhash.second);
        // if (it == m_bblhash2stats->end() || m_bblhash2stats->find(bblhash) == m_bblhash2stats->end()) {
        //     PrintStats(std::cout);
        //     printf("tid=%d size=%lu hash=%lx %lx\n", tid, m_bblhash2stats->size(), bblhash.first, bblhash.second);
        //     auto it3 = m_bblhash2stats->find(bblhash);
        //     std::cout << (it0 != m_bblhash2stats->end()) << std::endl;
        //     std::cout << (it != m_bblhash2stats->end()) << std::endl;
        //     std::cout << (it2 != m_bblhash2stats->end()) << std::endl;
        //     std::cout << (it3 != m_bblhash2stats->end()) << std::endl;
        //     assert(it != m_bblhash2stats->end());
        // }
        auto it = m_bblhash2stats->end();
        int cnt = 0;
        // TODO: In Sniper, somehow we need to find multiple times
        // to get the correct bblstats very ocassionally, this does not
        // cause fatal errors currently, but we need to fix this later.
        while (it == m_bblhash2stats->end()) {
            UUID bblhash = m_current_bblhash->back();
            it = m_bblhash2stats->find(bblhash);
            cnt++;
        }
        if (cnt > 1) {
            printf("tid=%d size=%lu hash=%lx %lx\n", tid, m_bblhash2stats->size(), it->first.first, it->first.second);
        }

        return it->second;
    }

    UUID GetCurrentBBLHash()
    {
        return m_current_bblhash->back();
    }

    void *stats_addr = NULL;
    size_t stats_size = 0;

    void BBLStart(uint64_t hi, uint64_t lo)
    {
        UUID bblhash = UUID(hi, lo);
        auto it = m_bblhash2stats->find(bblhash);
        
        if (it == m_bblhash2stats->end()) {
            BBLStats *stats = new BBLStats(GLOBAL_BBLID, bblhash);
            m_bblhash2stats->insert(std::make_pair(bblhash, stats));
        }
        m_current_bblhash->push_back(bblhash);
    }

    void BBLEnd(uint64_t hi, uint64_t lo)
    {
        UUID bblhash = UUID(hi, lo);
        // printf("%d: %lx %lx %lx %lx\n", tid, bblhash.first, bblhash.second, m_current_bblhash->back().first, m_current_bblhash->back().second);
        if (bblhash != m_current_bblhash->back()) 
        {
            printf("BBLEnd annotator not match: %d %lx %lx %lx %lx\n", tid, bblhash.first, bblhash.second, m_current_bblhash->back().first, m_current_bblhash->back().second);
        }
        assert(bblhash == m_current_bblhash->back());
        m_current_bblhash->pop_back();
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
        UUID bblhash = m_current_bblhash->back();
        BBLStats *bblstats = GetBBLStats(bblhash);
        bblstats->elapsed_time += (COST)time / 1e6;
        bblstats->instruction_count += instr;
    }

    void AddMemory(uint64_t memory_access)
    {
        UUID bblhash = m_current_bblhash->back();
        GetBBLStats(bblhash)->memory_access += memory_access;
    }

    // time unit is FS (1e-6 NS)
    void AddOffloadingTime(uint64_t time)
    {
        m_pim_time += (COST)time / 1e6;
    }

    void InsertSegOnHit(uintptr_t tag, bool is_store)
    {
        UUID bblhash = m_current_bblhash->back();
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
        BBLStats *bblstats = GetBBLStats(bblhash);
        seg->insert(bblstats);
        // int32_t threadcount = _storage->_cost_package->_thread_count;
        // if (threadcount > seg->getCount())
        seg->setCount(1);
        // split then insert on store
        if (is_store)
        {
            m_data_reuse->UpdateTrie(m_data_reuse->getRoot(), seg);
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
        m_data_reuse->UpdateTrie(m_data_reuse->getRoot(), seg);
        seg->clear();
    }

    void MergeStatsMap(UUIDHashMap<BBLStats *> &statsmap)
    {
        for (auto it = m_bblhash2stats->begin(); it != m_bblhash2stats->end(); ++it) {
            auto p = statsmap.find(it->first);
            if (p == statsmap.end()) {
                BBLStats *stats = new BBLStats(GLOBAL_BBLID, it->first);
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
    void GenerateBBLID(UUIDHashMap<BBLStats *> &statsmap)
    {
        std::vector<BBLStats *> sorted;
        SortStatsMap(statsmap, sorted);
        for (BBLID i = 0; i < (int)sorted.size(); ++i) {
            statsmap[sorted[i]->bblhash]->bblid = i;
        }
    }

    void AssignBBLID(UUIDHashMap<BBLStats *> &statsmap)
    {

        for (auto it = m_bblhash2stats->begin(); it != m_bblhash2stats->end(); ++it) {
            UUID bblhash = it->second->bblhash;
            auto p = statsmap.find(bblhash);
            assert(p != statsmap.end());
            it->second->bblid = p->second->bblid;
        }
    }

    void PrintStats(std::ostream &ofs)
    {
        std::vector<BBLStats *> sorted;
        SortStatsMap(*m_bblhash2stats, sorted);

        ofs << HORIZONTAL_LINE << std::endl;
        ofs << "Thread " << tid << std::endl;
        ofs << std::setw(7) << "BBLID"
            << std::setw(15) << "Time(ns)"
            << std::setw(15) << "Instruction"
            << std::setw(15) << "Memory Access"
            << std::setw(18) << "Hash(hi)"
            << std::setw(18) << "Hash(lo)"
            << std::endl;
        for (auto it = sorted.begin(); it != sorted.end(); ++it)
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

    void PrintPIMTime(std::ostream &ofs)
    {
        ofs << m_pim_time << std::endl;
    }
    // void PrintDataReuseDotGraph(std::ostream &ofs)
    // {
    //     m_data_reuse->PrintDotGraph(ofs);
    // }

    void PrintDataReuseSegments(std::ostream &ofs)
    {
        ofs << HORIZONTAL_LINE << std::endl;
        ofs << "Core " << tid << std::endl;
        m_data_reuse->PrintAllSegments(ofs,
            [](BBLStats *stats) { return stats->bblid; });
    }

  private:
    UUIDHashMap<COST> m_bblhash2cputime;

  public:
    void AddCPUTime(uint64_t time)
    {
        UUID bblhash = m_current_bblhash->back();
        m_bblhash2cputime[bblhash] += (COST)time / 1e6;
    }
};

} // namespace PIMProf

#endif // __STATS_H__