//===- [Yizhou]                                      ------------*- C++ -*-===//
//
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
} // namespace PIMProf

#endif // __DATAREUSE_H__