#ifndef __PINUTIL_H__
#define __PINUTIL_H__

#include "pin.H"
#include "INIReader.h"

namespace PIMProf {
    typedef UINT32 CACHE_STATS;
    typedef FLT64 COST;
    typedef UINT32 BBLID;
    

    static const UINT32 MAX_COST_SITE = 2;
    enum CostSite {
        CPU = 0,
        PIM = 1,
        INVALID = 0x3fffffff // a placeholder that does not count as a cost site
    };
    static const std::string CostSiteName[MAX_COST_SITE] = { "CPU", "PIM" };

    static const BBLID GLOBALBBLID = 0x7FFFFFFF;

    enum ACCESS_TYPE
    {
        ACCESS_TYPE_LOAD,
        ACCESS_TYPE_STORE,
        ACCESS_TYPE_NUM
    };

    typedef std::pair<BBLID, ACCESS_TYPE> BBLOP;

    class DataReuseSegment {
      friend class DataReuse;
      private:
        BBLID _headID;
        std::set<BBLID> _set;

      public:
        inline DataReuseSegment() {
            _headID = GLOBALBBLID;
        }

        inline size_t size() {
            return _set.size();
        }

        inline VOID insert(BBLID bblid) {
            if (_set.empty())
                _headID = bblid;
            _set.insert(bblid);
        }

        inline VOID clear() {
            _headID = GLOBALBBLID;
            _set.clear();
        }


        inline VOID setHead(BBLID head) {
            _headID = head;
        }

        inline BBLID getHead() {
            return _headID;
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

    class TrieNode {
      public:
        bool _isLeaf;
        std::map<BBLID, TrieNode *> _children;
        TrieNode *_parent;
        INT64 _count;
      public:
        inline TrieNode() {
            _isLeaf = false;
            _parent = NULL;
            _count = 0;
        }

        
    };

    inline INT32 INIErrorMsg(INT32 error, const string &filename, std::ostream &out) 
    {
        if (error == 0)
            return error;
        out << "################################################################################\n";
        out << "## ";
        if (error == -1) {
            out << "PIMProf: .ini file open error." << std::endl;
        }
        else if (error == -2) {
            out << "PIMProf: .ini file memory allocation for parsing error." << std::endl;
        } 
        
        else if (error > 0) {
            out << "PIMProf: .ini file parsing failure on line "
                << error 
                << "." << std::endl;
        }
        out << "## Filename: " << filename << std::endl;
        out << "################################################################################\n";
        return error;
    }
}

#endif // __PINUTIL_H__