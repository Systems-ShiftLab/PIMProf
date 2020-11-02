//===- [Yizhou]                                      ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#ifndef __PIMPROF_DATAREUSE_H__
#define __PIMPROF_DATAREUSE_H__

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
#include <map>
#include <algorithm>
#include <unordered_map>
#include <cassert>

#include "PinUtil.h"

namespace PIMProf
{
/* ===================================================================== */
/* DataReuseSegment */
/* ===================================================================== */
typedef std::pair<BBLID, ACCESS_TYPE> BBLOP;

class DataReuseSegment
{
    friend class DataReuse;

private:
    BBLID _headID;
    std::set<BBLID> _set;
    int _count;

public:
    inline DataReuseSegment()
    {
        _headID = -1;
        _count = 1;
    }

    inline size_t size() const { return _set.size(); }

    inline void insert(BBLID bblid)
    {
        if (_set.empty())
            _headID = bblid;
        _set.insert(bblid);
    }

    inline void insert(DataReuseSegment &seg)
    {
        _set.insert(seg._set.begin(), seg._set.end());
    }

    inline std::vector<BBLID> diff(DataReuseSegment &seg)
    {
        std::vector<BBLID> result;
        std::set_difference(
            _set.begin(), _set.end(),
            seg.begin(), seg.end(),
            std::inserter(result, result.end()));
        return result;
    }

    inline void clear()
    {
        _headID = -1;
        _set.clear();
        _count = 1;
    }

    inline std::set<BBLID>::iterator begin() { return _set.begin(); }
    inline std::set<BBLID>::iterator end() { return _set.end(); }
    inline void setHead(BBLID head) { _headID = head; }
    inline BBLID getHead() const { return _headID; }
    inline void setCount(int count) { _count = count; }
    inline int getCount() const { return _count; }

    inline bool operator==(DataReuseSegment &rhs)
    {
        return (_headID == rhs._headID && _set == rhs._set);
    }

    std::ostream &print(std::ostream &out)
    {
        out << "head = " << (int64_t)_headID << ", "
            << "count = " << _count << " | ";
        for (auto it = _set.begin(); it != _set.end(); it++)
        {
            out << (int64_t)*it << " ";
        }
        out << std::endl;
        return out;
    }
};

/* ===================================================================== */
/* TrieNode */
/* ===================================================================== */
class TrieNode
{
public:
    // the leaf node stores the head of the segment
    bool _isLeaf;
    std::map<BBLID, TrieNode *> _children;
    BBLID _curID;
    TrieNode *_parent;
    int64_t _count;

public:
    inline TrieNode()
    {
        _isLeaf = false;
        _parent = NULL;
        _count = 0;
    }
};

/* ===================================================================== */
/* DataReuse */
/* ===================================================================== */
/// We split a reuse chain into multiple segments that starts
/// with a W and ends with a W, for example:
/// A reuse chain: R W R R R R W R W W R R W R
/// can be splitted into: R W | R R R R W | R W | W | R R W | R
/// this is stored as segments that starts with a W and ends with a W:
/// R W; W R R R R W; W R W; W W; W R R W; W R

/// If all BB in a segment are executed in the same place,
/// then there is no reuse cost;
/// If the initial W is on PIM and there are subsequent R/W on CPU,
/// then this segment contributes to a flush of PIM and data fetch from CPU;
/// If the initial W is on CPU and there are subsequent R/W on PIM,
/// then this segment contributes to a flush of CPU and data fetch from PIM.
class DataReuse
{
private:
    TrieNode *_root;
    std::vector<TrieNode *> _leaves;

public:
    DataReuse() { _root = new TrieNode(); }
    ~DataReuse() { DeleteTrie(_root); }

public:
    void UpdateTrie(TrieNode *root, const DataReuseSegment *seg)
    {
        // A reuse chain segment of size 1 can be removed
        if (seg->size() <= 1)
            return;

        // seg->print(std::cout);

        TrieNode *curNode = root;
        std::set<BBLID>::iterator it = seg->_set.begin();
        std::set<BBLID>::iterator eit = seg->_set.end();
        for (; it != eit; it++)
        {
            BBLID curID = *it;
            TrieNode *temp = curNode->_children[curID];
            if (temp == NULL)
            {
                temp = new TrieNode();
                temp->_parent = curNode;
                temp->_curID = curID;
                curNode->_children[curID] = temp;
            }
            curNode = temp;
        }
        TrieNode *temp = curNode->_children[seg->_headID];
        if (temp == NULL)
        {
            temp = new TrieNode();
            temp->_parent = curNode;
            temp->_curID = seg->_headID;
            curNode->_children[seg->_headID] = temp;
            _leaves.push_back(temp);
        }
        temp->_isLeaf = true;
        temp->_count += seg->getCount();
    }

    void DeleteTrie(TrieNode *root)
    {
        if (!root->_isLeaf)
        {
            std::map<BBLID, TrieNode *>::iterator it = root->_children.begin();
            std::map<BBLID, TrieNode *>::iterator eit = root->_children.end();
            for (; it != eit; it++)
            {
                DataReuse::DeleteTrie(it->second);
            }
        }
        delete root;
    }

    void ExportSegment(DataReuseSegment *seg, TrieNode *leaf)
    {
        assert(leaf->_isLeaf);
        seg->setHead(leaf->_curID);
        seg->setCount(leaf->_count);

        TrieNode *temp = leaf;
        while (temp->_parent != NULL)
        {
            seg->insert(temp->_curID);
            temp = temp->_parent;
        }
    }

    void PrintDotGraphHelper(std::ostream &out, TrieNode *root, int parent, int &count)
    {
        int64_t curID = root->_curID;
        if (root->_isLeaf)
        {
            out << "    V_" << count << " [shape=box, label=\"head = " << curID << "\n cnt = " << root->_count << "\"];" << std::endl;
            out << "    V_" << parent << " -> V_" << count << ";" << std::endl;
            parent = count;
            count++;
        }
        else
        {
            out << "    V_" << count << " [label=\"" << curID << "\"];" << std::endl;
            out << "    V_" << parent << " -> V_" << count << ";" << std::endl;
            parent = count;
            count++;
            auto it = root->_children.begin();
            auto eit = root->_children.end();
            for (; it != eit; it++)
            {
                DataReuse::PrintDotGraphHelper(out, it->second, parent, count);
            }
        }
    }

    std::ostream &PrintDotGraph(std::ostream &out)
    {
        int parent = 0;
        int count = 1;
        out << "digraph trie {" << std::endl;
        auto it = _root->_children.begin();
        auto eit = _root->_children.end();
        out << "    V_0"
            << " [label=\"root\"];" << std::endl;
        for (; it != eit; it++)
        {
            DataReuse::PrintDotGraphHelper(out, it->second, parent, count);
        }
        out << "}" << std::endl;
        return out;
    }

    std::ostream &PrintAllSegments(std::ostream &out)
    {
        for (auto it = _leaves.begin(); it != _leaves.end(); it++)
        {
            DataReuseSegment seg;
            ExportSegment(&seg, *it);
            seg.print(out);
        }
        return out;
    }

    inline TrieNode *getRoot() { return _root; }
    inline std::vector<TrieNode *> &getLeaves() { return _leaves; }
};

/* ===================================================================== */
/* PIMProf Thread Data Collection */
/* ===================================================================== */

class PIMProfHashFunc
{
public:
    // assuming UUID is already murmurhash-ed.
    std::size_t operator()(const UUID &key) const
    {
        size_t result = key.first ^ key.second;
        return result;
    }
};

class PIMProfBBLStats
{
public:
    BBLID bblid;
    UUID bblhash;
    uint64_t elapsed_time; // store the nanosecond count of each basic block
    uint64_t instruction_count;
    uint64_t memory_access;

    PIMProfBBLStats(
        BBLID _bblid = 0,
        UUID _bblhash = UUID(0, 0),
        uint64_t _elapsed_time = 0,
        uint64_t _instruction_count = 0,
        uint64_t _memory_access = 0)
        : bblid(_bblid), bblhash(_bblhash), elapsed_time(_elapsed_time), instruction_count(_instruction_count), memory_access(_memory_access)
    {
    }
};

class PIMProfThreadStats
{
private:
    int tid;
    bool m_using_pim;
    std::vector<BBLID> m_current_bblid;
    uint64_t m_pim_time;

    // all class objects need to be stored in pointer form,
    // otherwise Sniper will somehow deallocate them unexpectedly.
    std::unordered_map<UUID, BBLID, PIMProfHashFunc> m_bblhash2bblid;
    std::vector<PIMProfBBLStats *> m_bblid2stats;
    PIMProfBBLStats *m_globalbblstats;

    std::unordered_map<uint64_t, DataReuseSegment *> m_tag2seg;
    DataReuse *m_data_reuse;

public:
    PIMProfThreadStats(int _tid = 0)
        : tid(_tid)
    {
        m_using_pim = false;
        m_pim_time = 0;
        // UUID(GLOBAL_BBLID, GLOBAL_BBLID) is the region outside main function.
        m_current_bblid.push_back(GLOBAL_BBLID);
        // UUID(0, 0) is the region that is inside main function but outside
        // any other BBL, we assign this region as BBL 0.
        m_bblhash2bblid.insert(std::make_pair(UUID(0, 0), 0));
        m_bblid2stats.push_back(new PIMProfBBLStats(0, UUID(0, 0)));
        m_globalbblstats = new PIMProfBBLStats(GLOBAL_BBLID, UUID(GLOBAL_BBLID, GLOBAL_BBLID));
        m_data_reuse = new DataReuse();
    }

    ~PIMProfThreadStats()
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
    bool PIMProfIsUsingPIM() { return m_using_pim; }
    int64_t PIMProfGetCurrentBBLID() { return m_current_bblid.back(); }

    PIMProfBBLStats *PIMProfGetBBLStats(BBLID bblid)
    {
        return (bblid == GLOBAL_BBLID ? m_globalbblstats : m_bblid2stats[bblid]);
    }

    UUID PIMProfGetCurrentBBLHash()
    {
        BBLID bblid = m_current_bblid.back();
        return PIMProfGetBBLStats(bblid)->bblhash;
    }

    void PIMProfBBLStart(uint64_t hi, uint64_t lo)
    {
        UUID bblhash = UUID(hi, lo);
        auto it = m_bblhash2bblid.find(bblhash);
        BBLID bblid = GLOBAL_BBLID;
        if (it == m_bblhash2bblid.end())
        {
            bblid = m_bblhash2bblid.size();
            m_bblhash2bblid.insert(std::make_pair(bblhash, bblid));

            m_bblid2stats.push_back(new PIMProfBBLStats(bblid, bblhash));
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

    void PIMProfBBLEnd(uint64_t hi, uint64_t lo)
    {
        UUID bblhash = UUID(hi, lo);
        auto it = m_bblhash2bblid.find(bblhash);
        assert(it != m_bblhash2bblid.end());
        assert(m_current_bblid.back() == it->second);
        m_current_bblid.pop_back();
    }

    void PIMProfOffloadStart(uint64_t hi, uint64_t type)
    {
        m_using_pim = true;
        // type is used to distinguish actual BBL start and end
        // since the start of a BBL could be the end of offloading
        // our compiling tool only provide the high bits of bblhash in this case
        if (type == 0)
        {
            PIMProfBBLStart(hi, 0);
        }
        else
        {
            PIMProfBBLEnd(hi, 0);
        }
    }

    void PIMProfOffloadEnd(uint64_t hi, uint64_t type)
    {
        m_using_pim = false;
        if (type == 0)
        {
            PIMProfBBLStart(hi, 0);
        }
        else
        {
            PIMProfBBLEnd(hi, 0);
        }
    }

    void PIMProfAddTimeInstruction(uint64_t time, uint64_t instr)
    {
        BBLID bblid = m_current_bblid.back();
        PIMProfGetBBLStats(bblid)->elapsed_time += time;
        PIMProfGetBBLStats(bblid)->instruction_count += instr;
    }

    void PIMProfAddMemory(uint64_t memory_access)
    {
        BBLID bblid = m_current_bblid.back();
        PIMProfGetBBLStats(bblid)->memory_access += memory_access;
    }

    void PIMProfAddOffloadingTime(uint64_t time)
    {
        m_pim_time += time;
    }

    void PIMProfInsertSegOnHit(uintptr_t tag, bool is_store)
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

    void PIMProfSplitSegOnMiss(uintptr_t tag)
    {
        DataReuseSegment *seg;
        auto it = m_tag2seg.find(tag);
        if (it == m_tag2seg.end())
            return; // ignore it if there is no existing segment
        seg = it->second;
        m_data_reuse->UpdateTrie(m_data_reuse->getRoot(), seg);
        seg->clear();
    }

    void PIMProfPrintStats(std::ostream &ofs)
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
                << PIMProfGetBBLStats(bblid)->elapsed_time << " "
                << PIMProfGetBBLStats(bblid)->instruction_count << " "
                << PIMProfGetBBLStats(bblid)->memory_access << std::endl;
        }
        ofs << tid << " " << std::hex 
            << m_globalbblstats->bblhash.first << " " << m_globalbblstats->bblhash.second << " " << std::dec
            << m_globalbblstats->elapsed_time << " "
            << m_globalbblstats->instruction_count << " "
            << m_globalbblstats->memory_access << std::endl;
    }

    void PIMProfPrintPIMTime(std::ostream &ofs)
    {
        ofs << m_pim_time << std::endl;
    }
    void PIMProfPrintDataReuseDotGraph(std::ostream &ofs)
    {
        m_data_reuse->PrintDotGraph(ofs);
    }
    void PIMProfPrintDataReuseSegments(std::ostream &ofs)
    {
        m_data_reuse->PrintAllSegments(ofs);
    }
};

} // namespace PIMProf

#endif // __PIMPROF_DATAREUSE_H__