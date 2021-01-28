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
class BBLSwitchCount
{
private:
    std::unordered_map<Ty, int> _elem2idx;
    std::vector<Ty> _idx2elem;
    std::vector<int> _total_count;
    std::vector<std::vector<int>> _count;

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

    inline void insert(Ty from, Ty to, int count=1)
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
        for (auto it = seg->_set.begin(); it != seg->_set.end(); ++it)
        {
            Ty cur = *it;
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
            for (auto it = root->_children.begin(); it != root->_children.end(); ++it)
            {
                DeleteTrie(it->second);
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
            auto it = root->_children.begin();
            auto eit = root->_children.end();
            for (; it != eit; ++it)
            {
                DataReuse::PrintDotGraphHelper(out, it->second, parent, count, get_id);
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
        for (auto it = _root->_children.begin(); it != _root->_children.end(); ++it)
        {
            DataReuse::PrintDotGraphHelper(out, it->second, parent, count, get_id);
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