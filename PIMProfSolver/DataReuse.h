//===- DataReuse.h - Data reuse class template ------------------*- C++ -*-===//
// Type Ty used here must be either a fundamental data type or a pointer
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#ifndef __DATAREUSE_H__
#define __DATAREUSE_H__

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

#include "Util.h"

namespace PIMProf
{
/* ===================================================================== */
/* BBLSwitchCount */
/* ===================================================================== */
template <class Ty>
class SwitchCountMatrix
{
private:
    std::unordered_map<Ty, uint64_t> _elem2idx;
    std::vector<Ty> _idx2elem;
    std::vector<uint64_t> _total_count;
    std::vector<std::vector<uint64_t>> _count;

public:
    inline void createElem(const Ty elem)
    {
        auto it = _elem2idx.find(elem);
        if (it == _elem2idx.end()) {
            _idx2elem.push_back(elem);
            _elem2idx.insert(std::make_pair(elem, _idx2elem.size() - 1));
        }
    }

    inline size_t getIdx(const Ty elem)
    {
        auto it = _elem2idx.find(elem);
        assert(it != _elem2idx.end());
        return it->second;
    }

    inline Ty getElem(const size_t idx)
    {
        assert(idx < _idx2elem.size());
        return _idx2elem[idx];
    }

    inline void insert(Ty from, Ty to, uint64_t count=1)
    {
        createElem(from);
        createElem(to);
        size_t fromidx = getIdx(from);
        size_t toidx = getIdx(to);
        if(_count.size() <= fromidx) {
            _count.resize(fromidx + 1);
            _total_count.resize(fromidx + 1, 0);
        }
        if (_count[fromidx].size() <= toidx) {
            _count[fromidx].resize(toidx + 1, 0);
        }
        _count[fromidx][toidx] += count;
        _total_count[fromidx] += count;
    }

    std::ostream &print(std::ostream &out, BBLID (*get_id)(Ty))
    {
        for (size_t fromidx = 0; fromidx < _count.size(); ++fromidx) {
            if (_total_count[fromidx] > 0) {
                out << "from = " << get_id(getElem(fromidx)) << " | ";
                for (size_t toidx = 0; toidx < _count[fromidx].size(); ++toidx) {
                    if (_count[fromidx][toidx] > 0) {
                        out << get_id(getElem(toidx)) << ":" << _count[fromidx][toidx] << " ";
                    }
                }
                out << std::endl;
            }
        }
        return out;
    }
};

class SwitchCountList{
    public:
    class SwitchCountRow {
    public:
        int64_t _fromidx;
        std::vector<std::pair<int64_t, uint64_t>> _toidxvec; // <toidx, count>

        SwitchCountRow(int64_t fromidx, std::vector<std::pair<int64_t, uint64_t>> toidxvec={})
        : _fromidx(fromidx),
          _toidxvec(toidxvec)
        {}

        inline std::vector<std::pair<int64_t, uint64_t>>::iterator begin() { return _toidxvec.begin(); }
        inline std::vector<std::pair<int64_t, uint64_t>>::iterator end() { return _toidxvec.end(); }

        inline const std::vector<std::pair<int64_t, uint64_t>>::const_iterator begin() const { return _toidxvec.begin(); }
        inline const std::vector<std::pair<int64_t, uint64_t>>::const_iterator end() const { return _toidxvec.end(); }

        COST Cost(const DECISION &decision, const COST switch_cost[MAX_COST_SITE]) {
            if (_toidxvec.size() == 0) return 0;
            COST result = 0;
            for (auto &elem : _toidxvec) {
                uint64_t toidx = elem.first;
                uint64_t count = elem.second;
                if (decision[_fromidx] != INVALID && decision[toidx] != INVALID && decision[_fromidx] != decision[toidx]) {
                    result += switch_cost[decision[_fromidx]] * count;
                }
            }
            return result;
        }

        // descending sort by count
        void Sort() {
            std::sort(_toidxvec.begin(), _toidxvec.end(), 
                [](std::pair<int64_t, uint64_t> l, std::pair<int64_t, uint64_t> r) { return l.second > r.second; });
        }
    };
private:
    std::vector<SwitchCountRow> _count;
public:

    inline std::vector<PIMProf::SwitchCountList::SwitchCountRow>::iterator begin() { return _count.begin(); }
    inline std::vector<PIMProf::SwitchCountList::SwitchCountRow>::iterator end() { return _count.end(); }

    inline const std::vector<PIMProf::SwitchCountList::SwitchCountRow>::const_iterator begin() const { return _count.begin(); }
    inline const std::vector<PIMProf::SwitchCountList::SwitchCountRow>::const_iterator end() const { return _count.end(); }

    inline SwitchCountRow &getRow(BBLID fromidx) {
        return _count[fromidx];
    }

    inline void RowInsert(BBLID fromidx, std::vector<std::pair<int64_t, uint64_t>> toidxvec)
    {
        while((BBLID)_count.size() <= fromidx) {
            _count.push_back(SwitchCountRow(_count.size()));
        }
        _count[fromidx] = SwitchCountRow(fromidx, toidxvec);
    }

    void Sort() {
        for (auto &row : _count) {
            row.Sort();
        }
    }

    std::ostream &print(std::ostream &out)
    {
        for (auto &row : _count) {
            if (row._toidxvec.size() == 0) continue;
            out << "from = " << row._fromidx << " | ";
            for (auto &elem : row._toidxvec) {
                out << elem.first << ":" << elem.second << " ";
            }
            out << std::endl;
        }
        return out;
    }

    std::ostream &printSwitch(std::ostream &out, const DECISION &decision, const COST switch_cost[MAX_COST_SITE])
    {
        for (auto &row : _count) {
            COST cost = row.Cost(decision, switch_cost);
            if (cost == 0) continue;
            CostSite fromsite = decision[row._fromidx];
            out << "cost = " << cost << ", ";
            out << "from = " << row._fromidx << getCostSiteString(fromsite) << " | ";
            for (auto &elem : row._toidxvec) {
                BBLID toidx = elem.first;
                CostSite tosite = decision[toidx];
                uint64_t count = elem.second;
                if (fromsite != INVALID && tosite != INVALID && fromsite != tosite) {
                    out << toidx << getCostSiteString(tosite) << ":" << count << " ";
                }
            }
            out << std::endl;
        }
        return out;
    }
};


/* ===================================================================== */
/* DataReuseSegment */
/* ===================================================================== */
typedef std::pair<BBLID, ACCESS_TYPE> BBLOP;

template <class Ty>
class DataReuseSegment
{
    template <class Tz> friend class DataReuse;

private:
    Ty _head;
    std::set<Ty> _set;
    uint64_t _count;

public:
    inline DataReuseSegment() {
        _count = 1;
    }

    inline size_t size() const { return _set.size(); }

    inline void insert(Ty elem)
    {
        if (_set.empty())
            _head = elem;
        _set.insert(elem);
    }

    inline void insert(DataReuseSegment &seg)
    {
        _set.insert(seg._set.begin(), seg._set.end());
    }

    inline std::vector<Ty> diff(DataReuseSegment &seg)
    {
        std::vector<Ty> result;
        std::set_difference(
            _set.begin(), _set.end(),
            seg.begin(), seg.end(),
            std::inserter(result, result.end()));
        return result;
    }

    inline void clear()
    {
        _set.clear();
        _count = 1;
    }

    inline typename std::set<Ty>::iterator begin() { return _set.begin(); }
    inline typename std::set<Ty>::iterator end() { return _set.end(); }
    inline void setHead(Ty head) { _head = head; }
    inline Ty getHead() const { return _head; }
    inline void setCount(uint64_t count) { _count = count; }
    inline uint64_t getCount() const { return _count; }

    inline bool operator==(DataReuseSegment &rhs)
    {
        return (_head == rhs._head && _set == rhs._set);
    }

    // the printing function needs to know how to index the elements of type Ty, so it accepts a function with prototype:
    // BBLID get_id(Ty elem);
    std::ostream &print(std::ostream &out, BBLID (*get_id)(Ty))
    {
        out << "head = " << get_id(_head) << ", "
            << "count = " << _count << " | ";
        for (auto it = _set.begin(); it != _set.end(); ++it)
        {
            out << get_id(*it) << " ";
        }
        out << std::endl;
        return out;
    }
};

/* ===================================================================== */
/* TrieNode */
/* ===================================================================== */
template <class Ty>
class TrieNode
{
public:
    // the leaf node stores the head of the segment
    bool _isLeaf;
    std::map<Ty, TrieNode *> _children;
    Ty _cur;
    TrieNode *_parent;
    uint64_t _count;

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
template <class Ty>
class DataReuse
{
private:
    TrieNode<Ty> *_root;
    std::vector<TrieNode<Ty> *> _leaves;

public:
    DataReuse() { _root = new TrieNode<Ty>(); }
    ~DataReuse() { DeleteTrie(_root); }
    inline TrieNode<Ty> *getRoot() { return _root; }
    inline std::vector<TrieNode<Ty> *> &getLeaves() { return _leaves; }

public:
    void UpdateTrie(TrieNode<Ty> *root, const DataReuseSegment<Ty> *seg)
    {
        // A reuse chain segment of size 1 can be removed
        if (seg->size() <= 1)
            return;

        // seg->print(std::cout);

        TrieNode<Ty> *curNode = root;
        for (auto cur : seg->_set)
        {
            TrieNode<Ty> *temp = curNode->_children[cur];
            if (temp == NULL)
            {
                temp = new TrieNode<Ty>();
                temp->_parent = curNode;
                temp->_cur = cur;
                curNode->_children[cur] = temp;
            }
            curNode = temp;
        }
        TrieNode<Ty> *temp = curNode->_children[seg->_head];
        if (temp == NULL)
        {
            temp = new TrieNode<Ty>();
            temp->_parent = curNode;
            temp->_cur = seg->_head;
            curNode->_children[seg->_head] = temp;
            _leaves.push_back(temp);
        }
        temp->_isLeaf = true;
        assert(temp->_count + seg->getCount() >= temp->_count); // detect overflow
        temp->_count += seg->getCount();
    }

    void DeleteTrie(TrieNode<Ty> *root)
    {
        if (!root->_isLeaf)
        {
            for (auto it : root->_children)
            {
                DeleteTrie(it.second);
            }
        }
        delete root;
    }

    void ExportSegment(DataReuseSegment<Ty> *seg, TrieNode<Ty> *leaf)
    {
        assert(leaf->_isLeaf);
        seg->setHead(leaf->_cur);
        seg->setCount(leaf->_count);

        TrieNode<Ty> *temp = leaf;
        while (temp->_parent != NULL)
        {
            seg->insert(temp->_cur);
            temp = temp->_parent;
        }
    }

    // Sort leaves by _count in descending order
    void SortLeaves() {
        std::sort(_leaves.begin(), _leaves.end(),
            [] (const TrieNode<Ty> *lhs, const TrieNode<Ty> *rhs) { return lhs->_count > rhs->_count; });
    }

    // the printing function needs to know how to index the elements of type Ty, so it accepts a function with prototype:
    // BBLID get_id(Ty elem);
    void PrintDotGraphHelper(std::ostream &out, TrieNode<Ty> *root, int parent, int &count, BBLID (*get_id)(Ty))
    {
        BBLID cur = get_id(root->_cur);
        if (root->_isLeaf)
        {
            out << "    V_" << count << " [shape=box, label=\"head = " << cur << "\n cnt = " << root->_count << "\"];" << std::endl;
            out << "    V_" << parent << " -> V_" << count << ";" << std::endl;
            parent = count;
            count++;
        }
        else
        {
            out << "    V_" << count << " [label=\"" << cur << "\"];" << std::endl;
            out << "    V_" << parent << " -> V_" << count << ";" << std::endl;
            parent = count;
            count++;

            for (auto it : root->_children)
            {
                DataReuse::PrintDotGraphHelper(out, it.second, parent, count, get_id);
            }
        }
    }

    std::ostream &PrintDotGraph(std::ostream &out, BBLID (*get_id)(Ty))
    {
        int parent = 0;
        int count = 1;
        out << "digraph trie {" << std::endl;
        out << "    V_0"
            << " [label=\"root\"];" << std::endl;
        for (auto it : _root->_children)
        {
            DataReuse::PrintDotGraphHelper(out, it.second, parent, count, get_id);
        }
        out << "}" << std::endl;
        return out;
    }

    std::ostream &PrintAllSegments(std::ostream &out, BBLID (*get_id)(Ty))
    {
        for (auto it : _leaves)
        {
            DataReuseSegment<Ty> seg;
            ExportSegment(&seg, it);
            seg.print(out, get_id);
        }
        return out;
    }

    std::ostream &PrintBBLOccurrence(std::ostream &out, BBLID (*get_id)(Ty))
    {
        // occurrence[bblid][seg_length].first stores seg_length
        // occurrence[bblid][seg_length].second stores count
        std::vector<std::vector<std::pair<int, int>>> occurrence;
        std::vector<int> total_occurrence;
        std::vector<int> length_dist;
        for (auto it : _leaves) {
            DataReuseSegment<Ty> seg;
            ExportSegment(&seg, it);
            size_t seg_length = seg.size();

            for (auto elem : seg) {
                BBLID bblid = get_id(elem);
                if (occurrence.size() <= bblid) {
                    occurrence.resize(bblid + 1);
                    total_occurrence.resize(bblid + 1);
                }
                if (occurrence[bblid].size() <= seg_length) {
                    occurrence[bblid].resize(seg_length + 1);
                }
                if (length_dist.size() <= seg_length) {
                    length_dist.resize(seg_length + 1);
                }
                occurrence[bblid][seg_length].first = seg_length;
                occurrence[bblid][seg_length].second += seg.getCount();
                total_occurrence[bblid] += seg.getCount();
                length_dist[seg_length] += seg.getCount();
            }
        }

        for (BBLID i = 0; i < total_occurrence.size(); ++i) {
            if (total_occurrence[i] > 0) {
                std::sort(occurrence[i].begin(), occurrence[i].end(),
                    [](std::pair<int, int> lhs, std::pair<int, int> rhs) { return lhs.second > rhs.second; });
                out << "BBLID = " << i << ", total = " << total_occurrence[i] << " | ";
                for (auto elem : occurrence[i]) {
                    if (elem.second <= 0) break;
                    out << "[" << elem.first << "]" << elem.second << " ";
                }
                out << std::endl;
            }
        }

        for (size_t i = 0; i < length_dist.size(); ++i) {
            out << "length = " << i << " | " << length_dist[i] << std::endl;
        }
        return out;
    }
};
} // namespace PIMProf

#endif // __DATAREUSE_H__
