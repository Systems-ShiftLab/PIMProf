#ifndef __DATAREUSE_H__
#define __DATAREUSE_H__

#include <stack>
#include <list>
#include <set>
#include "pin.H"

#include "PinUtil.h"

namespace PIMProf
{
/// We split a reuse chain into multiple segments that starts
/// with a W and ends with a W, for example:
/// A reuse chain: R W R R R R W R W W R R W R
/// can be splitted into: R W | R R R R W | R W | W | R R W | R
/// this is stored as segments that starts with a W and ends with a W:
/// R W; W R R R R W; W R W; W W; W R R W; W R

/// If all BB in a segment are executed in the same place,
/// then there is no reuse cost;
/// If the initial W is on PIM and there are subsequent R/W on CPU,
/// then this segment contributes to a fetch cost;
/// If the initial W is on CPU and there are subsequent R/W on PIM,
/// then this segment contributes to a flush cost.
class DataReuse {
    friend class CostSolver;
  private:
    static TrieNode *_root;
    static std::vector<TrieNode *> _leaves;

  public:
    DataReuse();
    ~DataReuse();
    static VOID UpdateTrie(TrieNode *root, DataReuseSegment &seg);
    static VOID DeleteTrie(TrieNode *root);
    static VOID ExportSegment(DataReuseSegment &seg, TrieNode *leaf);
    static VOID PrintTrie(std::ostream &out, TrieNode *root, int parent, int &count);
    static std::ostream &print(std::ostream &out, TrieNode *root);

    static inline TrieNode *getRoot() {
        return _root;
    }
    

};
} // namespace PIMProf


#endif // __DATAREUSE_H__