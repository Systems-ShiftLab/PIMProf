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
        uint64_t _elapsed_time = 0,
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
    bool m_using_pim;
    uint64_t m_pim_time;

    std::vector<UUID> m_current_bblhash;

    // all class objects need to be stored in pointer form,
    // otherwise Sniper will somehow deallocate them unexpectedly.
    UUIDHashMap<BBLStats *> m_bblhash2stats;

    std::unordered_map<uint64_t, PtrDataReuseSegment *> m_tag2seg;
    PtrDataReuse *m_data_reuse;

public:
    ThreadStats(int _tid = 0)
        : tid(_tid)
    {
        m_using_pim = false;
        m_pim_time = 0;
        // GLOBAL_BBLHASH is the region outside main function.
        m_bblhash2stats.insert(std::make_pair(GLOBAL_BBLHASH, new BBLStats()));
        m_current_bblhash.push_back(GLOBAL_BBLHASH);
        m_data_reuse = new PtrDataReuse();
    }

    ~ThreadStats()
    {
        for (auto it = m_bblhash2stats.begin(); it != m_bblhash2stats.end(); ++it)
        {
            delete it->second;
        }
        delete m_data_reuse;
        for (auto it = m_tag2seg.begin(); it != m_tag2seg.end(); ++it)
        {
            delete it->second;
        }
    }

    void setTid(int _tid) { tid = _tid; }
    bool IsUsingPIM() { return m_using_pim; }

    BBLStats *GetBBLStats(UUID temp)
    {
        
        // auto it0 = m_bblhash2stats.find(bblhash);
        // auto it = m_bblhash2stats.find(bblhash);
        // auto it2 = m_bblhash2stats.find(bblhash);
        // if (bblhash == UUID(0, 0))
        //     printf("tid=%d size=%lu hash=%lx %lx\n", tid, m_bblhash2stats.size(), bblhash.first, bblhash.second);
        // if (it == m_bblhash2stats.end() || m_bblhash2stats.find(bblhash) == m_bblhash2stats.end()) {
        //     PrintStats(std::cout);
        //     printf("tid=%d size=%lu hash=%lx %lx\n", tid, m_bblhash2stats.size(), bblhash.first, bblhash.second);
        //     auto it3 = m_bblhash2stats.find(bblhash);
        //     std::cout << (it0 != m_bblhash2stats.end()) << std::endl;
        //     std::cout << (it != m_bblhash2stats.end()) << std::endl;
        //     std::cout << (it2 != m_bblhash2stats.end()) << std::endl;
        //     std::cout << (it3 != m_bblhash2stats.end()) << std::endl;
        //     assert(it != m_bblhash2stats.end());
        // }
        auto it = m_bblhash2stats.end();
        int cnt = 0;
        // TODO: In Sniper, somehow we need to find multiple times
        // to get the correct bblstats very ocassionally, this does not
        // cause fatal errors currently, but we need to fix this later.
        while (it == m_bblhash2stats.end()) {
            UUID bblhash = m_current_bblhash.back();
            it = m_bblhash2stats.find(bblhash);
            cnt++;
        }
        if (cnt > 1) {
            printf("tid=%d size=%lu hash=%lx %lx\n", tid, m_bblhash2stats.size(), it->first.first, it->first.second);
        }

        return it->second;
    }

    UUID GetCurrentBBLHash()
    {
        return m_current_bblhash.back();
    }

    void BBLStart(uint64_t hi, uint64_t lo)
    {
        UUID bblhash = UUID(hi, lo);
        auto it = m_bblhash2stats.find(bblhash);
        // std::cout << tid << " " << std::hex << hi << " " << lo << std::endl;
        if (bblhash == UUID(0, 0)) {
            std::cout << tid << std::endl;
        }
        if (it == m_bblhash2stats.end()) {
            BBLStats *stats = new BBLStats(GLOBAL_BBLID, bblhash);
            // if (bblhash == UUID(0, 0)) {
            //     std::cout << "before: " << m_bblhash2stats.size() << std::endl;
            // }
            m_bblhash2stats.insert(std::make_pair(bblhash, stats));
            // if (bblhash == UUID(0, 0)) {
            //     std::cout << "after: " << m_bblhash2stats.size() << std::endl;
            // }
            
        }
        m_current_bblhash.push_back(bblhash);
    }

    void BBLEnd(uint64_t hi, uint64_t lo)
    {
        UUID bblhash = UUID(hi, lo);
        assert(bblhash == m_current_bblhash.back());
        m_current_bblhash.pop_back();
    }

    void OffloadStart(uint64_t hi, uint64_t type)
    {
        m_using_pim = true;
        // type is used to distinguish actual BBL start and end
        // since the start of a BBL could be the end of offloading
        // our compiling tool only provide the high bits of bblhash in this case
        if (type == 0)
        {
            BBLStart(hi, 0);
        }
        else
        {
            BBLEnd(hi, 0);
        }
    }

    void OffloadEnd(uint64_t hi, uint64_t type)
    {
        m_using_pim = false;
        if (type == 0)
        {
            BBLStart(hi, 0);
        }
        else
        {
            BBLEnd(hi, 0);
        }
    }

    // time unit is FS (1e-6 NS)
    void AddTimeInstruction(uint64_t time, uint64_t instr)
    {
        UUID bblhash = m_current_bblhash.back();
        BBLStats *bblstats = GetBBLStats(bblhash);
        bblstats->elapsed_time += (COST)time / 1e6;
        bblstats->instruction_count += instr;
    }

    void AddMemory(uint64_t memory_access)
    {
        UUID bblhash = m_current_bblhash.back();
        GetBBLStats(bblhash)->memory_access += memory_access;
    }

    // time unit is FS (1e-6 NS)
    void AddOffloadingTime(uint64_t time)
    {
        m_pim_time += (COST)time / 1e6;
    }

    void InsertSegOnHit(uintptr_t tag, bool is_store)
    {
        UUID bblhash = m_current_bblhash.back();
        PtrDataReuseSegment *seg;
        auto it = m_tag2seg.find(tag);
        if (it == m_tag2seg.end())
        {
            seg = new PtrDataReuseSegment();
            m_tag2seg.insert(std::make_pair(tag, seg));
        }
        else
        {
            seg = it->second;
        }

        seg->insert(GetBBLStats(bblhash));
        // int32_t threadcount = _storage->_cost_package->_thread_count;
        // if (threadcount > seg->getCount())
        seg->setCount(1);
        // split then insert on store
        if (is_store)
        {
            m_data_reuse->UpdateTrie(m_data_reuse->getRoot(), seg);
            seg->clear();
            seg->insert(GetBBLStats(bblhash));
            // if (threadcount > seg->getCount())
            seg->setCount(1);
        }
    }

    void SplitSegOnMiss(uintptr_t tag)
    {
        PtrDataReuseSegment *seg;
        auto it = m_tag2seg.find(tag);
        if (it == m_tag2seg.end())
            return; // ignore it if there is no existing segment
        seg = it->second;
        m_data_reuse->UpdateTrie(m_data_reuse->getRoot(), seg);
        seg->clear();
    }

    void MergeStatsMap(UUIDHashMap<BBLStats *> &statsmap)
    {
        for (auto it = m_bblhash2stats.begin(); it != m_bblhash2stats.end(); ++it) {
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

        for (auto it = m_bblhash2stats.begin(); it != m_bblhash2stats.end(); ++it) {
            UUID bblhash = it->second->bblhash;
            auto p = statsmap.find(bblhash);
            assert(p != statsmap.end());
            it->second->bblid = p->second->bblid;
        }
    }

    void PrintStats(std::ostream &ofs)
    {
        std::vector<BBLStats *> sorted;
        SortStatsMap(m_bblhash2stats, sorted);

        ofs << HORIZONTAL_LINE << std::endl;
        ofs << "Core " << tid << std::endl;
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
};

} // namespace PIMProf

#endif // __STATS_H__