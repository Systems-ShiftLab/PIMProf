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
#include "Util.h"
#include "CostPackage.h"


#include "INIReader.h"

/// @brief Checks if n is a power of 2.
/// @returns true if n is power of 2
static inline bool IsPower2(uint32_t n)
{
    return ((n & (n - 1)) == 0);
}

/// @brief Computes floor(log2(n))
/// Works by finding position of MSB set.
/// @returns -1 if n == 0.
static inline int32_t FloorLog2(uint32_t n)
{
    int32_t p = 0;

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
static inline int32_t CeilLog2(uint32_t n)
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

    inline void SetTag(ADDRINT tagaddr) {
       _tag = tagaddr;
    }
    inline ADDRINT GetTag() const { return _tag; }

    inline STORAGE *GetParent() { return _storage; }
};


class CACHE_SET
{
  protected:
    static const uint32_t MAX_ASSOCIATIVITY = 32;
    STORAGE *_storage;
  public:
    virtual ~CACHE_SET() {};
    virtual void SetAssociativity(uint32_t associativity) = 0;
    virtual uint32_t GetAssociativity(uint32_t associativity) = 0;
    virtual CACHE_TAG *Find(ADDRINT tagaddr) = 0;
    virtual CACHE_TAG *Replace(ADDRINT tagaddr) = 0;
    virtual void Flush() = 0;
};


class DIRECT_MAPPED : public CACHE_SET
{
  private:
    CACHE_TAG *_tag;

  public:
    inline DIRECT_MAPPED(STORAGE *storage, uint32_t associativity = 1) 
    {
        assert(associativity == 1);
        _tag = new CACHE_TAG(storage, 0);
        _storage = storage;
    }

    inline ~DIRECT_MAPPED()
    {
        delete _tag;
    }

    inline void SetAssociativity(uint32_t associativity) { assert(associativity == 1); }
    inline uint32_t GetAssociativity(uint32_t associativity) { return 1; }

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
    inline void Flush() { _tag->SetTag(0); }
};

/// @brief Cache set with round robin replacement
class ROUND_ROBIN : public CACHE_SET
{
  private:
    CACHE_TAG *_tags[MAX_ASSOCIATIVITY];
    uint32_t _tagsLastIndex;
    uint32_t _nextReplaceIndex;

  public:
    
    inline ROUND_ROBIN(STORAGE *storage, uint32_t associativity)
        : _tagsLastIndex(associativity - 1)
    {
        assert(associativity <= MAX_ASSOCIATIVITY);
        _nextReplaceIndex = _tagsLastIndex;

        for (int32_t index = _tagsLastIndex; index >= 0; index--)
        {
            _tags[index] = new CACHE_TAG(storage, 0);
        }
        _storage = storage;
    }

    inline ~ROUND_ROBIN()
    {
        for (int32_t index = _tagsLastIndex; index >= 0; index--)
        {
            delete _tags[index];
        }
    }

    inline void SetAssociativity(uint32_t associativity)
    {
        assert(associativity <= MAX_ASSOCIATIVITY);
        _tagsLastIndex = associativity - 1;
        _nextReplaceIndex = _tagsLastIndex;
    }

    inline uint32_t GetAssociativity(uint32_t associativity) { return _tagsLastIndex + 1; }

    inline CACHE_TAG *Find(ADDRINT tagaddr)
    {
        for (int32_t index = _tagsLastIndex; index >= 0; index--)
        {
            if (*_tags[index] == tagaddr)
                return _tags[index];
        }
        return NULL;
    }

    inline CACHE_TAG *Replace(ADDRINT tagaddr)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
        const uint32_t index = _nextReplaceIndex;

        _tags[index]->SetTag(tagaddr);
        // condition typically faster than modulo
        _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
        return _tags[index];
    }

    inline void Flush()
    {
        for (int32_t index = _tagsLastIndex; index >= 0; index--)
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
    inline LRU(STORAGE *storage, uint32_t associativity = MAX_ASSOCIATIVITY)
    {
        assert(associativity <= MAX_ASSOCIATIVITY);
        for (uint32_t i = 0; i < associativity; i++)
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

    inline void SetAssociativity(uint32_t associativity)
    {
        assert(associativity <= MAX_ASSOCIATIVITY);
        while (!_tags.empty())
        {
            CACHE_TAG *tag = _tags.back();
            delete tag;
            _tags.pop_back();
        }
        for (uint32_t i = 0; i < associativity; i++)
        {
            CACHE_TAG *tag = new CACHE_TAG(_storage, 0);
            _tags.push_back(tag);
        }
    }

    inline uint32_t GetAssociativity(uint32_t associativity)
    {
        return _tags.size();
    }

    inline CACHE_TAG *Find(ADDRINT tagaddr)
    {
        CacheTagList::iterator it = _tags.begin();
        CacheTagList::iterator eit = _tags.end();
        for (; it != eit; ++it)
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

    inline void Flush()
    {
        uint32_t associativity = _tags.size();
        while (!_tags.empty())
        {
            CACHE_TAG *tag = _tags.back();
            delete tag;
            _tags.pop_back();
        }
        for (uint32_t i = 0; i < associativity; i++)
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
    static const uint32_t HIT_MISS_NUM = 2;
    CACHE_STATS _access[MAX_ACCESS_TYPE][HIT_MISS_NUM];

  protected:
    STORAGE *_storage;
    STORAGE_LEVEL_BASE *_next_level;
    // _hitcost should be assigned to 0 if not used
    COST _hitcost[MAX_COST_SITE];

  public:
    virtual void AddMemCost(BBLID bblid, uint32_t simd_len) = 0;
    virtual bool Access(ADDRINT addr, uint32_t size, ACCESS_TYPE accessType, BBLID bblid, uint32_t simd_len) = 0;
    virtual bool AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType, BBLID bblid, uint32_t simd_len) = 0;

  protected:
    // input params
    const CostSite _cost_site;
    const StorageLevel _storage_level;

    CACHE_STATS SumAccess(bool hit) const
    {
        CACHE_STATS sum = 0;

        for (uint32_t accessType = 0; accessType < MAX_ACCESS_TYPE; accessType++)
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

    inline void InsertOnHit(ADDRINT tag, ACCESS_TYPE accessType, BBLID bblid);

    inline void SplitOnMiss(ADDRINT tag);

    inline std::string Name() const
    {
        return (CostSiteName[_cost_site] + "/" + StorageLevelName[_storage_level]);
    }

    /// @brief Stats output method
    virtual std::ostream &StatsLong(std::ostream &out) const;
    void CountMemoryCost(std::vector<COST> (&_BBL_cost)[MAX_COST_SITE], int cache_level) const;
};


/// @brief Templated cache class with specific cache set allocation policies
/// All that remains to be done here is allocate and deallocate the right
/// type of cache sets.
class CACHE_LEVEL : public STORAGE_LEVEL_BASE
{
  private:
    std::vector<CACHE_SET *> _sets;

  protected:
    const uint32_t _cacheSize;
    const uint32_t _lineSize;
    const uint32_t _associativity;
    std::string _replacement_policy;
    uint32_t STORE_ALLOCATION;
    uint32_t _numberOfFlushes;
    uint32_t _numberOfResets;

    // computed params
    const uint32_t _lineShift;
    const uint32_t _setIndexMask;
  
  // forbid copy constructor
  private:
    CACHE_LEVEL(const CACHE_LEVEL &);

  protected:
    uint32_t NumSets() const { return _setIndexMask + 1; }

  public:
    // constructors/destructors
    CACHE_LEVEL(STORAGE *storage, CostSite cost_site, StorageLevel storage_level, std::string policy, uint32_t cacheSize, uint32_t lineSize, uint32_t associativity, uint32_t allocation, COST hitcost[MAX_COST_SITE]);
    ~CACHE_LEVEL();

  public:
    // accessors
    inline uint32_t CacheSize() const { return _cacheSize; }
    inline uint32_t LineSize() const { return _lineSize; }
    inline uint32_t Associativity() const { return _associativity; }

    inline CACHE_STATS Flushes() const { return _numberOfFlushes; }
    inline CACHE_STATS Resets() const { return _numberOfResets; }

    inline void SplitAddress(const ADDRINT addr, ADDRINT &tagaddr, uint32_t &setIndex) const
    {
        tagaddr = addr >> _lineShift;
        setIndex = tagaddr & _setIndexMask;
    }

    inline void SplitAddress(const ADDRINT addr, ADDRINT &tagaddr, uint32_t &setIndex, uint32_t &lineIndex) const
    {
        const uint32_t lineMask = _lineSize - 1;
        lineIndex = addr & lineMask;
        SplitAddress(addr, tagaddr, setIndex);
    }

    inline void IncFlushCounter()
    {
        _numberOfFlushes += 1;
    }

    inline void IncResetCounter()
    {
        _numberOfResets += 1;
    }
  
  public:
    // modifiers
    void AddMemCost(BBLID bblid, uint32_t simd_len);

    void AddInstructionMemCost(BBLID bblid, uint32_t simd_len);

    /// Cache access from addr to addr+size-1/*!
    /// @return true if all accessed cache lines hit
    bool Access(ADDRINT addr, uint32_t size, ACCESS_TYPE accessType, BBLID bblid, uint32_t simd_len);

    /// Cache access at addr that does not span cache lines
    /// @return true if accessed cache line hits
    bool AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType, BBLID bblid, uint32_t simd_len);

    void Flush();
    void ResetStats();
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
    void AddMemCost(BBLID bblid, uint32_t simd_len);

    /// Cache access from addr to addr+size-1/*!
    /// @return true if all accessed cache lines hit
    bool Access(ADDRINT addr, uint32_t size, ACCESS_TYPE accessType, BBLID bblid, uint32_t simd_len);

    /// Cache access at addr that does not span cache lines
    /// @return true if accessed cache line hits
    bool AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType, BBLID bblid, uint32_t simd_len);
};

class STORAGE
{
  private:
    // point to the corresponding level of storage, if any.
    STORAGE_LEVEL_BASE *_storage[MAX_COST_SITE][MAX_LEVEL];
    STORAGE_LEVEL_BASE *_storage_top[MAX_COST_SITE][2];

  private:
    ADDRINT _last_icacheline[MAX_COST_SITE];

  public:
    /// Reference to PIMProfSolver data
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
    void WriteConfig(const std::string filename);

    std::ostream& WriteStats(std::ostream& out);
    void WriteStats(const std::string filename);

    /// Do on instruction cache reference
    void InstrCacheRef(ADDRINT addr, uint32_t size, BBLID bblid, uint32_t simd_len);

    /// Do on data cache reference
    void DataCacheRef(ADDRINT ip, ADDRINT addr, uint32_t size, ACCESS_TYPE accessType, BBLID bblid, uint32_t simd_len);

};

} // namespace PIMProf

#endif // __STORAGE_H__
