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
        UUID _bblhash = UUID(GLOBAL_BBLID, GLOBAL_BBLID),
        uint64_t _elapsed_time = 0,
        uint64_t _instruction_count = 0,
        uint64_t _memory_access = 0)
        : bblid(_bblid), bblhash(_bblhash), elapsed_time(_elapsed_time), instruction_count(_instruction_count), memory_access(_memory_access)
    {
    }

    BBLStats& operator += (const BBLStats& rhs) {
        this->elapsed_time += rhs.elapsed_time;
        this->instruction_count += rhs.instruction_count;
        this->memory_access += rhs.memory_access;
        return *this;
    }
};

typedef DataReuseSegment<BBLStats *> PtrDataReuseSegment;
typedef DataReuse<BBLStats *> PtrDataReuse;

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
    // UUIDHashMap<BBLID> m_bblhash2bblid;
    // std::vector<BBLStats *> m_bblid2stats;
    BBLStats *m_globalbblstats;

    std::unordered_map<uint64_t, PtrDataReuseSegment *> m_tag2seg;
    PtrDataReuse *m_data_reuse;

public:
    ThreadStats(int _tid = 0)
        : tid(_tid)
    {
        m_using_pim = false;
        m_pim_time = 0;
        // UUID(GLOBAL_BBLID, GLOBAL_BBLID) is the region outside main function.
        // UUID(0, 0) is the region that is inside main function but outside
        // any other BBL, we assign this region as BBL 0.
        m_current_bblhash.push_back(UUID(GLOBAL_BBLID, GLOBAL_BBLID));
        m_globalbblstats = new BBLStats(GLOBAL_BBLID, UUID(GLOBAL_BBLID, GLOBAL_BBLID));
        m_data_reuse = new PtrDataReuse();
    }

    ~ThreadStats()
    {
        for (auto it = m_bblhash2stats.begin(); it != m_bblhash2stats.end(); ++it)
        {
            delete it->second;
        }
        delete m_globalbblstats;
        delete m_data_reuse;
        for (auto it = m_tag2seg.begin(); it != m_tag2seg.end(); ++it)
        {
            delete it->second;
        }
    }

    void setTid(int _tid) { tid = _tid; }
    bool IsUsingPIM() { return m_using_pim; }

    BBLStats *GetBBLStats(UUID bblhash)
    {
        if (bblhash == UUID(GLOBAL_BBLID, GLOBAL_BBLID)) {
            return m_globalbblstats;
        }
        else {
            auto it = m_bblhash2stats.find(bblhash);
            assert(it != m_bblhash2stats.end());
            return it->second;
        }
    }

    UUID GetCurrentBBLHash()
    {
        return m_current_bblhash.back();
    }

    void BBLStart(uint64_t hi, uint64_t lo)
    {
        UUID bblhash = UUID(hi, lo);
        auto it = m_bblhash2stats.find(bblhash);
        if (it == m_bblhash2stats.end()) {
            BBLStats *stats = new BBLStats(GLOBAL_BBLID, bblhash);
            m_bblhash2stats.insert(std::make_pair(bblhash, stats));
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

    void AddTimeInstruction(uint64_t time, uint64_t instr)
    {
        UUID bblhash = m_current_bblhash.back();
        GetBBLStats(bblhash)->elapsed_time += time;
        GetBBLStats(bblhash)->instruction_count += instr;
    }

    void AddMemory(uint64_t memory_access)
    {
        UUID bblhash = m_current_bblhash.back();
        GetBBLStats(bblhash)->memory_access += memory_access;
    }

    void AddOffloadingTime(uint64_t time)
    {
        m_pim_time += time;
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

    // the pointers in sorted will point to the same location as the pointers in statsmap
    void GetSortedStats(UUIDHashMap<BBLStats *> &statsmap, std::vector<BBLStats *> &sorted)
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

    void MergeStatsMap(UUIDHashMap<BBLStats *> &statsmap)
    {
        for (auto it = m_bblhash2stats.begin(); it != m_bblhash2stats.end(); ++it) {
            auto p = statsmap.find(it->first);
            if (p == statsmap.end()) {
                BBLStats *stats = new BBLStats(GLOBAL_BBLID, it->first);
                *stats += *it->second;
                statsmap.insert(std::make_pair(it->first, stats));
            }
            else {
                // merge the stats
                *p->second += *it->second;
            }
        }
    }

    void GenerateBBLID(UUIDHashMap<BBLStats *> &statsmap)
    {
        std::vector<BBLStats *> sorted;
        GetSortedStats(statsmap, sorted);
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
        GetSortedStats(m_bblhash2stats, sorted);

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