//===- Cache.cpp - Cache implementation -------------------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#include <fstream>
#include <iostream>

#include "INIReader.h"
#include "Cache.h"

using namespace PIMProf;

std::string PIMProf::StringInt(UINT64 val, UINT32 width, CHAR padding)
{
    std::ostringstream ostr;
    ostr.setf(std::ios::fixed,std::ios::floatfield);
    ostr.fill(padding);
    ostr << std::setw(width) << val;
    return ostr.str();
}

std::string PIMProf::StringHex(UINT64 val, UINT32 width, CHAR padding)
{
    std::ostringstream ostr;
    ostr.setf(std::ios::fixed,std::ios::floatfield);
    ostr.fill(padding);
    ostr << std::setw(width) << std::hex << "0x" << val;
    return ostr.str();
}

std::string PIMProf::StringString(std::string val, UINT32 width, CHAR padding)
{
    std::ostringstream ostr;
    ostr.setf(std::ios::fixed,std::ios::floatfield);
    ostr.fill(padding);
    ostr << std::setw(width) << val;
    return ostr.str();
}

CACHE_LEVEL_BASE::CACHE_LEVEL_BASE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity)
  : _name(name),
    _cacheSize(cacheSize),
    _lineSize(lineSize),
    _associativity(associativity),
    _lineShift(FloorLog2(lineSize)),
    _setIndexMask((cacheSize / (associativity * lineSize)) - 1)
{

    ASSERTX(IsPower2(_lineSize));
    ASSERTX(IsPower2(_setIndexMask + 1));

    for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
    {
        _access[accessType][false] = 0;
        _access[accessType][true] = 0;
    }
}

/*!
 *  @brief Stats output method
 */
std::ostream & CACHE_LEVEL_BASE::StatsLong(std::ostream & out) const
{
    const UINT32 headerWidth = 19;
    const UINT32 numberWidth = 10;

    out << _name << ":" << std::endl;

    for (UINT32 i = 0; i < ACCESS_TYPE_NUM; i++)
    {
        const ACCESS_TYPE accessType = ACCESS_TYPE(i);

        std::string type(accessType == ACCESS_TYPE_LOAD ? "Load" : "Store");

        out << StringString(type + " Hits:      ", headerWidth)
            << StringInt(Hits(accessType), numberWidth) << std::endl;
        out << StringString(type + " Misses:    ", headerWidth)
            << StringInt(Misses(accessType), numberWidth) << std::endl;
        out << StringString(type + " Accesses:  ", headerWidth)
            << StringInt(Accesses(accessType), numberWidth) << std::endl;
        out << StringString(type + " Miss Rate: ", headerWidth)
            << StringFlt(100.0 * Misses(accessType) / Accesses(accessType), 2, numberWidth-1) << "%" << std::endl;
        out << std::endl;
    }

    out << StringString("Total Hits:      ", headerWidth, ' ')
        << StringInt(Hits(), numberWidth) << std::endl;
    out << StringString("Total Misses:    ", headerWidth, ' ')
        << StringInt(Misses(), numberWidth) << std::endl;
    out << StringString("Total Accesses:  ", headerWidth, ' ')
        << StringInt(Accesses(), numberWidth) << std::endl;
    out << StringString("Total Miss Rate: ", headerWidth, ' ')
        << StringFlt(100.0 * Misses() / Accesses(), 2, numberWidth-1) << "%" << std::endl;

    out << StringString("Flushes:         ", headerWidth, ' ')
        << StringInt(Flushes(), numberWidth) << std::endl;
    out << StringString("Stat Resets:     ", headerWidth, ' ')
        << StringInt(Resets(), numberWidth) << std::endl;

    out << std::endl;

    return out;
}

CACHE_LEVEL::CACHE_LEVEL(std::string name, std::string policy, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, UINT32 allocation)
    : CACHE_LEVEL_BASE(name, cacheSize, lineSize, associativity), _replacement_policy(policy), STORE_ALLOCATION(allocation)
{
    if (policy == "direct_mapped") {
        for (UINT32 i = 0; i < NumSets(); i++) {
            DIRECT_MAPPED *_set = new DIRECT_MAPPED();
            _set->SetAssociativity(associativity);
            _sets.push_back(_set);
        }
    }
    else if (policy == "round_robin") {
        for (UINT32 i = 0; i < NumSets(); i++) {
            ROUND_ROBIN *_set = new ROUND_ROBIN(associativity);
            _set->SetAssociativity(associativity);
            _sets.push_back(_set);
        }
    }
    else if (policy == "lru") {
        for (UINT32 i = 0; i < NumSets(); i++) {
            LRU *_set = new LRU(associativity);
            _set->SetAssociativity(associativity);
            _sets.push_back(_set);
        }
    }
    else {
        ASSERTX(0 && "Invalid cache replacement policy name!");
    }
}

CACHE_LEVEL::~CACHE_LEVEL()
{
    while (!_sets.empty()) {
        CACHE_SET *temp = _sets.back();
        if (temp != NULL)
            delete temp;
        _sets.pop_back();
    }
}

/*!
 *  @return true if all accessed cache lines hit
 */
BOOL CACHE_LEVEL::Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    const ADDRINT highAddr = addr + size;
    BOOL allHit = true;

    const ADDRINT lineSize = LineSize();
    const ADDRINT notLineMask = ~(lineSize - 1);
    do
    {
        CACHE_TAG tag;
        UINT32 setIndex;

        SplitAddress(addr, tag, setIndex);

        CACHE_SET *set = _sets[setIndex];

        BOOL localHit = set->Find(tag);
        allHit &= localHit;

        // on miss, loads always allocate, stores optionally
        if ((!localHit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
        {
            set->Replace(tag);
        }

        addr = (addr & notLineMask) + lineSize; // start of next cache line
    } while (addr < highAddr);

    _access[accessType][allHit]++;

    return allHit;
}

/*!
 *  @return true if accessed cache line hits
 */
BOOL CACHE_LEVEL::AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType)
{
    CACHE_TAG tag;
    UINT32 setIndex;

    SplitAddress(addr, tag, setIndex);

    CACHE_SET *set = _sets[setIndex];

    BOOL hit = set->Find(tag);

    // on miss, loads always allocate, stores optionally
    if ((!hit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
    {
        set->Replace(tag);
    }

    _access[accessType][hit]++;

    return hit;
}
/*!
 *  @return true if accessed cache line hits
 */
VOID CACHE_LEVEL::Flush()
{
    for (INT32 index = NumSets(); index >= 0; index--)
    {
        CACHE_SET *set = _sets[index];
        set->Flush();
    }
    IncFlushCounter();
}

VOID CACHE_LEVEL::ResetStats()
{
    for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
    {
        _access[accessType][false] = 0;
        _access[accessType][true] = 0;
    }
    IncResetCounter();
}

/* ===================================================================== */
/* Cache */
/* ===================================================================== */
const std::string CACHE::_name[CACHE::MAX_LEVEL] = {
        "ITLB", "DTLB", "IL1", "DL1", "UL2", "UL3"
    };
CACHE_LEVEL *CACHE::_cache[CACHE::MAX_LEVEL];

CACHE::CACHE()
{
}

CACHE::CACHE(const std::string filename)
{
    ReadConfig(filename);
}

CACHE::~CACHE()
{
    for (UINT32 i = 0; i < MAX_LEVEL; i++) {
        if(_cache[i] != NULL)
            delete _cache[i];
    }
}

VOID CACHE::ReadConfig(std::string filename)
{
    INIReader reader(filename);
    for (UINT32 i = 0; i < MAX_LEVEL; i++) {
        
        std::string name = _name[i];
        UINT32 linesize = reader.GetInteger(name, "linesize", -1);
        UINT32 cachesize = reader.GetInteger(name, "cachesize", -1);
        UINT32 associativity = reader.GetInteger(name, "associativity", -1);
        UINT32 allocation = reader.GetInteger(name, "allocation", -1);
        std::string policy = reader.Get(name, "policy", "");
        _cache[i] = new CACHE_LEVEL(name, policy, cachesize, linesize, associativity, allocation);
    }
}

std::ostream& CACHE::WriteConfig(std::ostream& out)
{
    // TODO
    return out;
}

VOID CACHE::WriteConfig(const std::string filename)
{
    std::ofstream out;
    out.open(filename.c_str(), ios_base::out);
    WriteConfig(out);
    out.close();
}

std::ostream& CACHE::WriteStats(std::ostream& out)
{
    std::cout << "wow" <<std::endl;
    for (UINT32 i = 0; i < MAX_LEVEL; i++) {
        _cache[i]->StatsLong(out);
    }
    return out;
}
VOID CACHE::WriteStats(const std::string filename)
{
    std::ofstream out;
    out.open(filename.c_str(), ios_base::out);
    WriteStats(out);
    out.close();
}

VOID CACHE::Ul2Access(ADDRINT addr, UINT32 size, CACHE_LEVEL_BASE::ACCESS_TYPE accessType)
{
    // second level unified cache
    const BOOL ul2Hit = _cache[UL2]->Access(addr, size, accessType);

    // third level unified cache
    if ( ! ul2Hit) _cache[UL3]->Access(addr, size, accessType);
}

VOID CACHE::InsRef(ADDRINT addr)
{
    const UINT32 size = 1; // assuming access does not cross cache lines
    const CACHE_LEVEL_BASE::ACCESS_TYPE accessType = CACHE_LEVEL_BASE::ACCESS_TYPE_LOAD;

    // ITLB
    _cache[ITLB]->AccessSingleLine(addr, accessType);

    // first level I-cache
    const BOOL il1Hit = _cache[IL1]->AccessSingleLine(addr, accessType);

    // second level unified Cache
    if ( ! il1Hit) Ul2Access(addr, size, accessType);
}


VOID CACHE::MemRefMulti(ADDRINT addr, UINT32 size, CACHE_LEVEL_BASE::ACCESS_TYPE accessType)
{
    // DTLB
    _cache[DTLB]->AccessSingleLine(addr, CACHE_LEVEL_BASE::ACCESS_TYPE_LOAD);

    // first level D-cache
    const BOOL dl1Hit = _cache[DL1]->Access(addr, size, accessType);

    // second level unified Cache
    if ( ! dl1Hit) Ul2Access(addr, size, accessType);
}

VOID CACHE::MemRefSingle(ADDRINT addr, UINT32 size, CACHE_LEVEL_BASE::ACCESS_TYPE accessType)
{
    // DTLB
    _cache[DTLB]->AccessSingleLine(addr, CACHE_LEVEL_BASE::ACCESS_TYPE_LOAD);

    // first level D-cache
    const BOOL dl1Hit = _cache[DL1]->AccessSingleLine(addr, accessType);

    // second level unified Cache
    if ( ! dl1Hit) Ul2Access(addr, size, accessType);
}