//===- [Yizhou]                                      ------------*- C++ -*-===//
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
    BBLID bblid;
    UUID bblhash;
    COST elapsed_time; // store the nanosecond count of each basic block
    uint64_t instruction_count;
    uint64_t memory_access;

    BBLStats(
        BBLID _bblid = 0,
        UUID _bblhash = UUID(0, 0),
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

class ThreadStats
{
private:
    int tid;
    bool m_using_pim;
    std::vector<BBLID> m_current_bblid;
    uint64_t m_pim_time;

    // all class objects need to be stored in pointer form,
    // otherwise Sniper will somehow deallocate them unexpectedly.
    UUIDHashMap<BBLID> m_bblhash2bblid;
    std::vector<BBLStats *> m_bblid2stats;
    BBLStats *m_globalbblstats;

    std::unordered_map<uint64_t, DataReuseSegment *> m_tag2seg;
    DataReuse *m_data_reuse;

public:
    ThreadStats(int _tid = 0)
        : tid(_tid)
    {
        m_using_pim = false;
        m_pim_time = 0;
        // UUID(GLOBAL_BBLID, GLOBAL_BBLID) is the region outside main function.
        m_current_bblid.push_back(GLOBAL_BBLID);
        // UUID(0, 0) is the region that is inside main function but outside
        // any other BBL, we assign this region as BBL 0.
        m_bblhash2bblid.insert(std::make_pair(UUID(0, 0), 0));
        m_bblid2stats.push_back(new BBLStats(0, UUID(0, 0)));
        m_globalbblstats = new BBLStats(GLOBAL_BBLID, UUID(GLOBAL_BBLID, GLOBAL_BBLID));
        m_data_reuse = new DataReuse();
    }

    ~ThreadStats()
    {
        for (auto it = m_bblid2stats.begin(); it != m_bblid2stats.end(); ++it)
        {
            delete *it;
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
    int64_t GetCurrentBBLID() { return m_current_bblid.back(); }

    BBLStats *GetBBLStats(BBLID bblid)
    {
        return (bblid == GLOBAL_BBLID ? m_globalbblstats : m_bblid2stats[bblid]);
    }

    UUID GetCurrentBBLHash()
    {
        BBLID bblid = m_current_bblid.back();
        return GetBBLStats(bblid)->bblhash;
    }

    void BBLStart(uint64_t hi, uint64_t lo)
    {
        UUID bblhash = UUID(hi, lo);
        auto it = m_bblhash2bblid.find(bblhash);
        BBLID bblid = GLOBAL_BBLID;
        if (it == m_bblhash2bblid.end())
        {
            bblid = m_bblhash2bblid.size();
            m_bblhash2bblid.insert(std::make_pair(bblhash, bblid));

            m_bblid2stats.push_back(new BBLStats(bblid, bblhash));
            if (bblhash.first <= 0x10000000)
            {
                printf("%lu %lu %lx %lx\n", bblid, m_bblid2stats.size(), bblhash.first, bblhash.second);
            }
        }
        else
        {
            bblid = it->second;
        }
        m_current_bblid.push_back(bblid);
    }

    void BBLEnd(uint64_t hi, uint64_t lo)
    {
        UUID bblhash = UUID(hi, lo);
        auto it = m_bblhash2bblid.find(bblhash);
        assert(it != m_bblhash2bblid.end());
        assert(m_current_bblid.back() == it->second);
        m_current_bblid.pop_back();
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
        BBLID bblid = m_current_bblid.back();
        GetBBLStats(bblid)->elapsed_time += time;
        GetBBLStats(bblid)->instruction_count += instr;
    }

    void AddMemory(uint64_t memory_access)
    {
        BBLID bblid = m_current_bblid.back();
        GetBBLStats(bblid)->memory_access += memory_access;
    }

    void AddOffloadingTime(uint64_t time)
    {
        m_pim_time += time;
    }

    void InsertSegOnHit(uintptr_t tag, bool is_store)
    {
        BBLID bblid = m_current_bblid.back();
        DataReuseSegment *seg;
        auto it = m_tag2seg.find(tag);
        if (it == m_tag2seg.end())
        {
            seg = new DataReuseSegment();
            m_tag2seg.insert(std::make_pair(tag, seg));
        }
        else
        {
            seg = it->second;
        }

        seg->insert(bblid);
        // int32_t threadcount = _storage->_cost_package->_thread_count;
        // if (threadcount > seg->getCount())
        seg->setCount(1);
        // split then insert on store
        if (is_store)
        {
            m_data_reuse->UpdateTrie(m_data_reuse->getRoot(), seg);
            seg->clear();
            seg->insert(bblid);
            // if (threadcount > seg->getCount())
            seg->setCount(1);
        }
    }

    void SplitSegOnMiss(uintptr_t tag)
    {
        DataReuseSegment *seg;
        auto it = m_tag2seg.find(tag);
        if (it == m_tag2seg.end())
            return; // ignore it if there is no existing segment
        seg = it->second;
        m_data_reuse->UpdateTrie(m_data_reuse->getRoot(), seg);
        seg->clear();
    }

    void PrintStats(std::ostream &ofs)
    {
        std::vector<std::pair<UUID, BBLID>> m_bblhash_sorted(m_bblhash2bblid.begin(), m_bblhash2bblid.end());
        std::sort(
            m_bblhash_sorted.begin(),
            m_bblhash_sorted.end(),
            [](std::pair<UUID, BBLID> &a, std::pair<UUID, BBLID> &b) { return a.first.first < b.first.first; });
        for (auto it = m_bblhash_sorted.begin(); it != m_bblhash_sorted.end(); ++it)
        {
            UUID bblhash = it->first;
            BBLID bblid = it->second;
            ofs << tid << " " << std::hex
                << bblhash.first << " " << bblhash.second << " " << std::dec
                << GetBBLStats(bblid)->elapsed_time << " "
                << GetBBLStats(bblid)->instruction_count << " "
                << GetBBLStats(bblid)->memory_access << std::endl;
        }
        ofs << tid << " " << std::hex 
            << m_globalbblstats->bblhash.first << " " << m_globalbblstats->bblhash.second << " " << std::dec
            << m_globalbblstats->elapsed_time << " "
            << m_globalbblstats->instruction_count << " "
            << m_globalbblstats->memory_access << std::endl;
    }

    void PrintPIMTime(std::ostream &ofs)
    {
        ofs << m_pim_time << std::endl;
    }
    void PrintDataReuseDotGraph(std::ostream &ofs)
    {
        m_data_reuse->PrintDotGraph(ofs);
    }
    void PrintDataReuseSegments(std::ostream &ofs)
    {
        ofs << HORIZONTAL_LINE << std::endl;
        ofs << "Core " << tid << std::endl;
        m_data_reuse->PrintAllSegments(ofs);
    }
};

} // namespace PIMProf

#endif // __STATS_H__