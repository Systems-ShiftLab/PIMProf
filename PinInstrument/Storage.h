//===- Storage.h - Cache implementation ---------------------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#ifndef __STORAGE_H__
#define __STORAGE_H__

#include <fstream>
#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <set>

#include "pin.H"
#include "PinUtil.h"
#include "CostPackage.h"


#include "INIReader.h"

/// @brief Checks if n is a power of 2.
/// @returns true if n is power of 2
static inline bool IsPower2(UINT32 n)
{
    return ((n & (n - 1)) == 0);
}

/// @brief Computes floor(log2(n))
/// Works by finding position of MSB set.
/// @returns -1 if n == 0.
static inline INT32 FloorLog2(UINT32 n)
{
    INT32 p = 0;

    if (n == 0)
        return -1;

    if (n & 0xffff0000)
    {
        p += 16;
        n >>= 16;
    }
    if (n & 0x0000ff00)
    {
        p += 8;
        n >>= 8;
    }
    if (n & 0x000000f0)
    {
        p += 4;
        n >>= 4;
    }
    if (n & 0x0000000c)
    {
        p += 2;
        n >>= 2;
    }
    if (n & 0x00000002)
    {
        p += 1;
    }

    return p;
}

/// @brief Computes ceil(log2(n))
/// Works by finding position of MSB set.
/// @returns -1 if n == 0.
static inline INT32 CeilLog2(UINT32 n)
{
    return FloorLog2(n - 1) + 1;
}


namespace PIMProf {

// forward declaration
class STORAGE;

/// @brief Cache tag
/// dynamic data structure should only be allocated on construction
/// and deleted on destruction
class CACHE_TAG
{
  private:
    ADDRINT _tag;
    STORAGE *_storage;

  public:
    CACHE_TAG(STORAGE *storage, ADDRINT tagaddr = 0)
    {
        _tag = tagaddr;
        _storage = storage;
    }

    inline bool operator == (const ADDRINT &rhs) const { return _tag == rhs; }

    inline VOID SetTag(ADDRINT tagaddr) {
       _tag = tagaddr;
    }
    inline ADDRINT GetTag() const { return _tag; }

    inline STORAGE *GetParent() { return _storage; }
};


class CACHE_SET
{
  protected:
    static const UINT32 MAX_ASSOCIATIVITY = 32;
    STORAGE *_storage;
  public:
    virtual ~CACHE_SET() {};
    virtual VOID SetAssociativity(UINT32 associativity) = 0;
    virtual UINT32 GetAssociativity(UINT32 associativity) = 0;
    virtual CACHE_TAG *Find(ADDRINT tagaddr) = 0;
    virtual CACHE_TAG *Replace(ADDRINT tagaddr) = 0;
    virtual VOID Flush() = 0;
};


class DIRECT_MAPPED : public CACHE_SET
{
  private:
    CACHE_TAG *_tag;

  public:
    inline DIRECT_MAPPED(STORAGE *storage, UINT32 associativity = 1) 
    {
        ASSERTX(associativity == 1);
        _tag = new CACHE_TAG(storage, 0);
        _storage = storage;
    }

    inline ~DIRECT_MAPPED()
    {
        delete _tag;
    }

    inline VOID SetAssociativity(UINT32 associativity) { ASSERTX(associativity == 1); }
    inline UINT32 GetAssociativity(UINT32 associativity) { return 1; }

    inline CACHE_TAG *Find(ADDRINT tagaddr) 
    {
        if (*_tag == tagaddr) {
            return _tag;
        }
        return NULL;
    }
    inline CACHE_TAG *Replace(ADDRINT tagaddr) 
    {
        _tag->SetTag(tagaddr);
        return _tag;
    }
    inline VOID Flush() { _tag->SetTag(0); }
};

/// @brief Cache set with round robin replacement
class ROUND_ROBIN : public CACHE_SET
{
  private:
    CACHE_TAG *_tags[MAX_ASSOCIATIVITY];
    UINT32 _tagsLastIndex;
    UINT32 _nextReplaceIndex;

  public:
    
    inline ROUND_ROBIN(STORAGE *storage, UINT32 associativity)
        : _tagsLastIndex(associativity - 1)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _nextReplaceIndex = _tagsLastIndex;

        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            _tags[index] = new CACHE_TAG(storage, 0);
        }
        _storage = storage;
    }

    inline ~ROUND_ROBIN()
    {
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            delete _tags[index];
        }
    }

    inline VOID SetAssociativity(UINT32 associativity)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _tagsLastIndex = associativity - 1;
        _nextReplaceIndex = _tagsLastIndex;
    }

    inline UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }

    inline CACHE_TAG *Find(ADDRINT tagaddr)
    {
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            if (*_tags[index] == tagaddr)
                return _tags[index];
        }
        return NULL;
    }

    inline CACHE_TAG *Replace(ADDRINT tagaddr)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
        const UINT32 index = _nextReplaceIndex;

        _tags[index]->SetTag(tagaddr);
        // condition typically faster than modulo
        _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
        return _tags[index];
    }

    inline VOID Flush()
    {
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            _tags[index]->SetTag(0);
        }
        _nextReplaceIndex = _tagsLastIndex;
    }
};

class LRU : public CACHE_SET
{
  public:
    typedef std::list<CACHE_TAG *> CacheTagList;

  private:
    // this is a fixed-size list where the size is the current associativity
    // front is MRU, back is LRU
    CacheTagList _tags;

  public:
    inline LRU(STORAGE *storage, UINT32 associativity = MAX_ASSOCIATIVITY)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        for (UINT32 i = 0; i < associativity; i++)
        {
            CACHE_TAG *tag = new CACHE_TAG(storage, 0);
            tag->SetTag(0);
            _tags.push_back(tag);
        }
        _storage = storage;
    }

    inline ~LRU()
    {
        while (!_tags.empty())
        {
            CACHE_TAG *tag = _tags.back();
            delete tag;
            _tags.pop_back();
        }
    }

    inline VOID SetAssociativity(UINT32 associativity)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        while (!_tags.empty())
        {
            CACHE_TAG *tag = _tags.back();
            delete tag;
            _tags.pop_back();
        }
        for (UINT32 i = 0; i < associativity; i++)
        {
            CACHE_TAG *tag = new CACHE_TAG(_storage, 0);
            _tags.push_back(tag);
        }
    }

    inline UINT32 GetAssociativity(UINT32 associativity)
    {
        return _tags.size();
    }

    inline CACHE_TAG *Find(ADDRINT tagaddr)
    {
        CacheTagList::iterator it = _tags.begin();
        CacheTagList::iterator eit = _tags.end();
        for (; it != eit; it++)
        {
            CACHE_TAG *tag = *it;
            // promote the accessed cache line to the front
            if (*tag == tagaddr)
            {
                _tags.erase(it);
                _tags.push_front(tag);
                return _tags.front();
            }
        }
        return NULL;
    }

    inline CACHE_TAG *Replace(ADDRINT tagaddr)
    {
        CACHE_TAG *tag = _tags.back();
        _tags.pop_back();
        tag->SetTag(tagaddr);
        _tags.push_front(tag);
        return tag;
    }

    inline VOID Flush()
    {
        UINT32 associativity = _tags.size();
        while (!_tags.empty())
        {
            CACHE_TAG *tag = _tags.back();
            delete tag;
            _tags.pop_back();
        }
        for (UINT32 i = 0; i < associativity; i++)
        {
            CACHE_TAG *tag = new CACHE_TAG(_storage, 0);
            _tags.push_back(tag);
        }
    }
};


namespace CACHE_ALLOC
{
enum STORE_ALLOCATION
{
    STORE_ALLOCATE,
    STORE_NO_ALLOCATE
};
}

/// @brief Generic base class of storage level; no allocate specialization, no cache set specialization; the memory is imitated with this class as well.
class STORAGE_LEVEL_BASE
{
    friend class STORAGE;
    friend class CACHE_LEVEL;
    friend class MEMORY_LEVEL;
  protected:
    static const UINT32 HIT_MISS_NUM = 2;
    CACHE_STATS _access[ACCESS_TYPE_NUM][HIT_MISS_NUM];

  protected:
    STORAGE *_storage;
    STORAGE_LEVEL_BASE *_next_level;
    // _hitcost should be assigned to zero if not used
    COST _hitcost[MAX_COST_SITE];

  public:
    virtual BOOL Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType) = 0;
    virtual BOOL AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType) = 0;

  protected:
    // input params
    const CostSite _cost_site;
    const StorageLevel _storage_level;

    CACHE_STATS SumAccess(bool hit) const
    {
        CACHE_STATS sum = 0;

        for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
        {
            sum += _access[accessType][hit];
        }

        return sum;
    }

  public:
    // constructors/destructors
    STORAGE_LEVEL_BASE(STORAGE *storage, CostSite cost_site, StorageLevel storage_level);
    virtual ~STORAGE_LEVEL_BASE() = default;

    CACHE_STATS Hits(ACCESS_TYPE accessType) const { return _access[accessType][true]; }
    CACHE_STATS Misses(ACCESS_TYPE accessType) const { return _access[accessType][false]; }
    CACHE_STATS Accesses(ACCESS_TYPE accessType) const { return Hits(accessType) + Misses(accessType); }
    CACHE_STATS Hits() const { return SumAccess(true); }
    CACHE_STATS Misses() const { return SumAccess(false); }
    CACHE_STATS Accesses() const { return Hits() + Misses(); }

    inline VOID InsertOnHit(ADDRINT tag, BBLID bblid, ACCESS_TYPE accessType);

    inline VOID SplitOnMiss(ADDRINT tag);

    inline std::string Name() const
    {
        return (CostSiteName[_cost_site] + "/" + StorageLevelName[_storage_level]);
    }

    /// @brief Stats output method
    virtual std::ostream &StatsLong(std::ostream &out) const;
    VOID CountMemoryCost(std::vector<COST> (&_BBL_cost)[MAX_COST_SITE], int cache_level) const;
};


/// @brief Templated cache class with specific cache set allocation policies
/// All that remains to be done here is allocate and deallocate the right
/// type of cache sets.
class CACHE_LEVEL : public STORAGE_LEVEL_BASE
{
  private:
    std::vector<CACHE_SET *> _sets;

  protected:
    const UINT32 _cacheSize;
    const UINT32 _lineSize;
    const UINT32 _associativity;
    std::string _replacement_policy;
    UINT32 STORE_ALLOCATION;
    UINT32 _numberOfFlushes;
    UINT32 _numberOfResets;

    // computed params
    const UINT32 _lineShift;
    const UINT32 _setIndexMask;
  
  // forbid copy constructor
  private:
    CACHE_LEVEL(const CACHE_LEVEL &);

  protected:
    UINT32 NumSets() const { return _setIndexMask + 1; }

  public:
    // constructors/destructors
    CACHE_LEVEL(STORAGE *storage, CostSite cost_site, StorageLevel storage_level, std::string policy, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, UINT32 allocation, COST hitcost[MAX_COST_SITE]);
    ~CACHE_LEVEL();

  public:
    // accessors
    inline UINT32 CacheSize() const { return _cacheSize; }
    inline UINT32 LineSize() const { return _lineSize; }
    inline UINT32 Associativity() const { return _associativity; }

    inline CACHE_STATS Flushes() const { return _numberOfFlushes; }
    inline CACHE_STATS Resets() const { return _numberOfResets; }

    inline VOID SplitAddress(const ADDRINT addr, ADDRINT &tagaddr, UINT32 &setIndex) const
    {
        tagaddr = addr >> _lineShift;
        setIndex = tagaddr & _setIndexMask;
    }

    inline VOID SplitAddress(const ADDRINT addr, ADDRINT &tagaddr, UINT32 &setIndex, UINT32 &lineIndex) const
    {
        const UINT32 lineMask = _lineSize - 1;
        lineIndex = addr & lineMask;
        SplitAddress(addr, tagaddr, setIndex);
    }

    inline VOID IncFlushCounter()
    {
        _numberOfFlushes += 1;
    }

    inline VOID IncResetCounter()
    {
        _numberOfResets += 1;
    }
  
  public:
    // modifiers
    VOID AddMemCost();

    /// Cache access from addr to addr+size-1/*!
    /// @return true if all accessed cache lines hit
    BOOL Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);

    /// Cache access at addr that does not span cache lines
    /// @return true if accessed cache line hits
    BOOL AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType);

    VOID Flush();
    VOID ResetStats();
    inline std::string getReplacementPolicy() {
        return _replacement_policy;
    }
    std::ostream &StatsLong(std::ostream &out) const;
};

class MEMORY_LEVEL : public STORAGE_LEVEL_BASE
{  
  // forbid copy constructor
  private:
    MEMORY_LEVEL(const MEMORY_LEVEL &);

  public:
    // constructors/destructors
    MEMORY_LEVEL(STORAGE *storage, CostSite cost_site, StorageLevel storage_level, COST hitcost[MAX_COST_SITE]);

    // modifiers
    VOID AddMemCost();

    /// Cache access from addr to addr+size-1/*!
    /// @return true if all accessed cache lines hit
    BOOL Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);

    /// Cache access at addr that does not span cache lines
    /// @return true if accessed cache line hits
    BOOL AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType);
};

class STORAGE
{
  private:
    // point to the corresponding level of storage, if any.
    STORAGE_LEVEL_BASE *_storage[MAX_COST_SITE][MAX_LEVEL];

  public:
    /// Reference to PinInstrument data
    CostPackage *_cost_package;

  public:
    STORAGE();
    ~STORAGE();
  public:
    void initialize(CostPackage *cost_package, ConfigReader &reader);

    void ReadConfig(ConfigReader &reader);

    /// Write the current cache config to ofstream or file.
    /// If no modification is made, then this will output the 
    /// default cache config PIMProf will use.
    std::ostream& WriteConfig(std::ostream& out);
    VOID WriteConfig(const std::string filename);

    std::ostream& WriteStats(std::ostream& out);
    VOID WriteStats(const std::string filename);

    VOID Ul2Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);

    /// Do on instruction cache reference
    VOID InstrCacheRef(ADDRINT addr);

    /// Do on multi-line data cache references
    VOID DataCacheRefMulti(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);

    /// Do on a single-line data cache reference
    VOID DataCacheRefSingle(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);
};

} // namespace PIMProf

#endif // __STORAGE_H__
