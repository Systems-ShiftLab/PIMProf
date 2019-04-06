//===- Cache.h - Cache implementation ---------------------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#ifndef __CACHE_H__
#define __CACHE_H__

#include <string>
#include <list>
#include <vector>

#include "pin.H"

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
    typedef UINT32 CACHE_STATS;
    typedef UINT32 BBLID;
/// @brief Cache tag - self clearing on creation
class CACHE_TAG
{
  private:
    ADDRINT _tag;
    BBLID _bblid;

  public:
    CACHE_TAG(ADDRINT tag = 0) { _tag = tag; }
    bool operator==(const CACHE_TAG &right) const { return _tag == right._tag; }
    operator ADDRINT() const { return _tag; }
};


class CACHE_SET
{
  protected:
    static const UINT32 MAX_ASSOCIATIVITY = 32;
  public:
    virtual ~CACHE_SET() {};
    virtual VOID SetAssociativity(UINT32 associativity) = 0;
    virtual UINT32 GetAssociativity(UINT32 associativity) = 0;
    virtual UINT32 Find(CACHE_TAG tag) = 0;
    virtual VOID Replace(CACHE_TAG tag) = 0;
    virtual VOID Flush() = 0;
};


class DIRECT_MAPPED : public CACHE_SET
{
  private:
    CACHE_TAG _tag;

  public:
    inline DIRECT_MAPPED(UINT32 associativity = 1) { ASSERTX(associativity == 1); }

    inline VOID SetAssociativity(UINT32 associativity) { ASSERTX(associativity == 1); }
    inline UINT32 GetAssociativity(UINT32 associativity) { return 1; }

    inline UINT32 Find(CACHE_TAG tag) { return (_tag == tag); }
    inline VOID Replace(CACHE_TAG tag) { _tag = tag; }
    inline VOID Flush() { _tag = 0; }
};

/// @brief Cache set with round robin replacement
class ROUND_ROBIN : public CACHE_SET
{
  private:
    CACHE_TAG _tags[MAX_ASSOCIATIVITY];
    UINT32 _tagsLastIndex;
    UINT32 _nextReplaceIndex;

  public:
    
    inline ROUND_ROBIN(UINT32 associativity)
        : _tagsLastIndex(associativity - 1)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _nextReplaceIndex = _tagsLastIndex;

        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            _tags[index] = CACHE_TAG(0);
        }
    }

    inline VOID SetAssociativity(UINT32 associativity)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _tagsLastIndex = associativity - 1;
        _nextReplaceIndex = _tagsLastIndex;
    }

    inline UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }

    inline UINT32 Find(CACHE_TAG tag)
    {
        bool result = true;

        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...
            if (_tags[index] == tag)
                goto end;
        }
        result = false;

    end:
        return result;
    }

    inline VOID Replace(CACHE_TAG tag)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
        const UINT32 index = _nextReplaceIndex;

        _tags[index] = tag;
        // condition typically faster than modulo
        _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
    }
    inline VOID Flush()
    {
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            _tags[index] = 0;
        }
        _nextReplaceIndex = _tagsLastIndex;
    }
};

class LRU : public CACHE_SET
{
  public:
    typedef std::list<CACHE_TAG> CacheTagList;

  private:
    // this is a fixed-size list where the size is the current associativity
    // front is MRU, back is LRU
    CacheTagList _tags;

  public:
    inline LRU(UINT32 associativity = MAX_ASSOCIATIVITY)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        for (UINT32 i = 0; i < associativity; i++)
        {
            _tags.push_back(CACHE_TAG(0));
        }
    }

    inline VOID SetAssociativity(UINT32 associativity)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _tags.clear();
        for (UINT32 i = 0; i < associativity; i++)
        {
            _tags.push_back(CACHE_TAG(0));
        }
    }

    inline UINT32 GetAssociativity(UINT32 associativity)
    {
        return _tags.size();
    }

    inline UINT32 Find(CACHE_TAG tag)
    {
        CacheTagList::iterator it = _tags.begin();
        CacheTagList::iterator eit = _tags.end();
        for (; it != eit; it++)
        {
            // promote the accessed cache line to the front
            if (*it == tag)
            {
                _tags.erase(it);
                _tags.push_front(tag);
                return true;
            }
        }
        return false;
    }

    inline VOID Replace(CACHE_TAG tag)
    {
        _tags.pop_back();
        _tags.push_front(tag);
    }

    inline VOID Flush()
    {
        UINT32 associativity = _tags.size();
        _tags.clear();
        for (UINT32 i = 0; i < associativity; i++)
        {
            _tags.push_back(CACHE_TAG(0));
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

/// @brief Generic cache base class; no allocate specialization, no cache set specialization
class CACHE_LEVEL_BASE
{
  public:
    // types, constants
    enum ACCESS_TYPE
    {
        ACCESS_TYPE_LOAD,
        ACCESS_TYPE_STORE,
        ACCESS_TYPE_NUM
    };

  protected:
    static const UINT32 HIT_MISS_NUM = 2;
    CACHE_STATS _access[ACCESS_TYPE_NUM][HIT_MISS_NUM];

  private:
    // input params
    const std::string _name;
    const UINT32 _cacheSize;
    const UINT32 _lineSize;
    const UINT32 _associativity;
    UINT32 _numberOfFlushes;
    UINT32 _numberOfResets;

    // computed params
    const UINT32 _lineShift;
    const UINT32 _setIndexMask;

    CACHE_STATS SumAccess(bool hit) const
    {
        CACHE_STATS sum = 0;

        for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
        {
            sum += _access[accessType][hit];
        }

        return sum;
    }

  protected:
    UINT32 NumSets() const { return _setIndexMask + 1; }

  public:
    // constructors/destructors
    CACHE_LEVEL_BASE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity);

    // accessors
    UINT32 CacheSize() const { return _cacheSize; }
    UINT32 LineSize() const { return _lineSize; }
    UINT32 Associativity() const { return _associativity; }
    //
    CACHE_STATS Hits(ACCESS_TYPE accessType) const { return _access[accessType][true]; }
    CACHE_STATS Misses(ACCESS_TYPE accessType) const { return _access[accessType][false]; }
    CACHE_STATS Accesses(ACCESS_TYPE accessType) const { return Hits(accessType) + Misses(accessType); }
    CACHE_STATS Hits() const { return SumAccess(true); }
    CACHE_STATS Misses() const { return SumAccess(false); }
    CACHE_STATS Accesses() const { return Hits() + Misses(); }

    CACHE_STATS Flushes() const { return _numberOfFlushes; }
    CACHE_STATS Resets() const { return _numberOfResets; }

    VOID SplitAddress(const ADDRINT addr, CACHE_TAG &tag, UINT32 &setIndex) const
    {
        tag = addr >> _lineShift;
        setIndex = tag & _setIndexMask;
    }

    VOID SplitAddress(const ADDRINT addr, CACHE_TAG &tag, UINT32 &setIndex, UINT32 &lineIndex) const
    {
        const UINT32 lineMask = _lineSize - 1;
        lineIndex = addr & lineMask;
        SplitAddress(addr, tag, setIndex);
    }

    VOID IncFlushCounter()
    {
        _numberOfFlushes += 1;
    }

    VOID IncResetCounter()
    {
        _numberOfResets += 1;
    }

    /// @brief Stats output method
    std::ostream &StatsLong(std::ostream &out) const;
};


/// @brief Templated cache class with specific cache set allocation policies
/// All that remains to be done here is allocate and deallocate the right
/// type of cache sets.
class CACHE_LEVEL : public CACHE_LEVEL_BASE
{
  private:
    std::string _replacement_policy;
    std::vector<CACHE_SET *> _sets;
    UINT32 STORE_ALLOCATION;
  
  // forbid copy constructor
  private:
    CACHE_LEVEL(const CACHE_LEVEL &);

  public:
    // constructors/destructors
    CACHE_LEVEL(std::string name, std::string policy, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, UINT32 allocation);
    ~CACHE_LEVEL();

    // modifiers
    
    /// Cache access from addr to addr+size-1/*!
    /// @return true if all accessed cache lines hit
    BOOL Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);

    /// Cache access at addr that does not span cache lines
    /// @return true if accessed cache line hits
    BOOL AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType);

    // @return true if accessed cache line hits
    VOID Flush();
    VOID ResetStats();
    inline std::string getReplacementPolicy() {
        return _replacement_policy;
    }
};

class CACHE 
{
  public:
    static const UINT32 MAX_LEVEL = 6;
    enum {
        ITLB, DTLB, IL1, DL1, UL2, UL3
    };
    static const std::string _name[MAX_LEVEL];
  private:
    static CACHE_LEVEL *_cache[MAX_LEVEL];

  // forbid copy constructor
  private:
    CACHE(const CACHE &);

  public:
    CACHE();
    CACHE(const std::string filename);
    ~CACHE();

    VOID ReadConfig(std::string filename);

    /// Write the current cache config to ofstream or file.
    /// If no modification is made, then this will output the 
    /// default cache config PIMProf will use.
    std::ostream& WriteConfig(std::ostream& out);
    VOID WriteConfig(const std::string filename);

    std::ostream& WriteStats(std::ostream& out);
    VOID WriteStats(const std::string filename);

    VOID Ul2Access(ADDRINT addr, UINT32 size, CACHE_LEVEL_BASE::ACCESS_TYPE accessType);

    /// Do on instruction cache reference
    VOID InsRef(ADDRINT addr);

    /// Do on multi-line data cache references
    VOID MemRefMulti(ADDRINT addr, UINT32 size, CACHE_LEVEL_BASE::ACCESS_TYPE accessType);

    /// Do on a single-line data cache reference
    VOID MemRefSingle(ADDRINT addr, UINT32 size, CACHE_LEVEL_BASE::ACCESS_TYPE accessType);
};

std::string StringInt(UINT64 val, UINT32 width = 0, CHAR padding = ' ');
std::string StringHex(UINT64 val, UINT32 width = 0, CHAR padding = ' ');
std::string StringString(std::string val, UINT32 width = 0, CHAR padding = ' ');

} // namespace PIMProf

#endif // __CACHE_H__
