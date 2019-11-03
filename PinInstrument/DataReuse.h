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
#include "pin.H"

#include "PinUtil.h"

namespace PIMProf
{
/* ===================================================================== */
/* DataReuseSegment */
/* ===================================================================== */
typedef std::pair<BBLID, ACCESS_TYPE> BBLOP;

class DataReuseSegment {
  friend class DataReuse;
  private:
    BBLID _headID;
    std::set<BBLID> _set;
    int _count;

  public:
    inline DataReuseSegment() {
        _headID = GLOBALBBLID;
        _count = 1;
    }

    inline size_t size() const {
        return _set.size();
    }

    inline VOID insert(BBLID bblid) {
        if (_set.empty())
            _headID = bblid;
        _set.insert(bblid);
    }

    inline VOID insert(DataReuseSegment &seg) {
        _set.insert(seg._set.begin(), seg._set.end());
    }

    inline std::vector<BBLID> diff(DataReuseSegment &seg) {
        std::vector<BBLID> result;
        std::set_difference(
            _set.begin(), _set.end(),
            seg.begin(), seg.end(),
            std::inserter(result, result.end())
        );
        return result;
    }

    inline VOID clear() {
        _headID = GLOBALBBLID;
        _set.clear();
    }

    inline std::set<BBLID>::iterator begin() {
        return _set.begin();
    }

    inline std::set<BBLID>::iterator end() {
        return _set.end();
    }

    inline VOID setHead(BBLID head) {
        _headID = head;
    }

    inline BBLID getHead() const {
        return _headID;
    }

    inline VOID setCount(int count) {
        _count = count;
    }

    inline int getCount() const {
        return _count;
    }

    inline BOOL operator == (DataReuseSegment &rhs) {
        return (_headID == rhs._headID && _set == rhs._set);
    }

    inline std::ostream &print(std::ostream &out) {
        out << "{ ";
        out << _headID << " | ";
        for (auto it = _set.begin(); it != _set.end(); it++) {
            out << *it << ", ";
        }
        out << "}";
        out << std::endl;
        return out;
    }

    
};

/* ===================================================================== */
/* TrieNode */
/* ===================================================================== */
class TrieNode {
  public:
    bool _isLeaf;
    std::map<BBLID, TrieNode *> _children;
    BBLID _curID;
    TrieNode *_parent;
    INT64 _count;
  public:
    inline TrieNode() {
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
class DataReuse {
  private:
    TrieNode *_root;
    std::vector<TrieNode *> _leaves;

  public:
    DataReuse();
    ~DataReuse();

  public:
    void UpdateTrie(TrieNode *root, DataReuseSegment &seg);
    void DeleteTrie(TrieNode *root);
    void ExportSegment(DataReuseSegment &seg, TrieNode *leaf);
    void PrintTrie(std::ostream &out, TrieNode *root, int parent, int &count);
    std::ostream &print(std::ostream &out, TrieNode *root);

    inline TrieNode *getRoot() {
        return _root;
    }

    inline std::vector<TrieNode *> &getLeaves() {
        return _leaves;
    }

    void ReadConfig(ConfigReader &reader);

};
} // namespace PIMProf


#endif // __DATAREUSE_H__