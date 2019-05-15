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
#include "PinInstrument.h"
#include "Cache.h"

using namespace PIMProf;

/* ===================================================================== */
/* Static data structure */
/* ===================================================================== */

const std::string CACHE::_name[CACHE::MAX_LEVEL] = {
        "ITLB", "DTLB", "IL1", "DL1", "UL2", "UL3"
    };
CACHE_LEVEL *CACHE::_cache[CACHE::MAX_LEVEL];

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

/* ===================================================================== */
/* Cache tag */
/* ===================================================================== */

/* ===================================================================== */
/* Base class for cache level */
/* ===================================================================== */

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


/* ===================================================================== */
/* Cache level */
/* ===================================================================== */

CACHE_LEVEL::CACHE_LEVEL(std::string name, std::string policy, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, UINT32 allocation, COST hitcost[MAX_COST_SITE])
    : CACHE_LEVEL_BASE(name, cacheSize, lineSize, associativity), _replacement_policy(policy), STORE_ALLOCATION(allocation)
{
    // NumSets = cacheSize / (associativity * lineSize)
    if (policy == "direct_mapped") {
        for (UINT32 i = 0; i < NumSets(); i++) {
            DIRECT_MAPPED *_set = new DIRECT_MAPPED(associativity);
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
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        _hitcost[i] = hitcost[i];
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


VOID CACHE_LEVEL::addmemcost(BOOL hit, CACHE_LEVEL *lvl)
{
    if (hit) {
        BBLID bblid = PinInstrument::GetCurrentBBL();
        if (bblid != GLOBALBBLID) {
            for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
                CostSolver::_BBL_memory_cost[i][bblid] += lvl->_hitcost[i];
            }
        }
    }
    else {
        BBLID bblid = PinInstrument::GetCurrentBBL();
        if (bblid != GLOBALBBLID) {
            if (lvl->_name == "UL3") {
                for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
                    CostSolver::_BBL_memory_cost[i][bblid] += CostSolver::_memory_cost[i];
                }
            }
        }
    }
} 


BOOL CACHE_LEVEL::Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    const ADDRINT highAddr = addr + size;
    BOOL allHit = true;

    const ADDRINT lineSize = LineSize();
    const ADDRINT notLineMask = ~(lineSize - 1);
    do
    {
        ADDRINT tagaddr;
        UINT32 setIndex;

        SplitAddress(addr, tagaddr, setIndex);

        CACHE_SET *set = _sets[setIndex];
        CACHE_TAG *tag = set->Find(tagaddr);
        BOOL localhit = (tag != NULL);
        allHit &= localhit;

        // on miss, loads always allocate, stores optionally
        if ((!localhit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
        {
            tag = set->Replace(tagaddr);
            // CostSolver::AddDataReuseCost(tag->GetBBLOperation());
            tag->ClearBBLOperation();
        }

        // tag == NULL means that accessType is STORE and STORE_ALLOCATION is NO ALLOCATE
        if (tag != NULL) {
            tag->InsertBBLOperation(PinInstrument::GetCurrentBBL(), accessType);
        }

        addr = (addr & notLineMask) + lineSize; // start of next cache line

        CACHE_LEVEL::addmemcost(localhit, this);

    } while (addr < highAddr);


    _access[accessType][allHit]++;

    return allHit;
}


BOOL CACHE_LEVEL::AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType)
{
    ADDRINT tagaddr;
    UINT32 setIndex;

    SplitAddress(addr, tagaddr, setIndex);

    CACHE_SET *set = _sets[setIndex];

    CACHE_TAG *tag = set->Find(tagaddr);
    BOOL hit = (tag != NULL);

    // on miss, loads always allocate, stores optionally
    if ((!hit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
    {
        tag = set->Replace(tagaddr);
        // CostSolver::AddDataReuseCost(tag->GetBBLOperation());
        tag->ClearBBLOperation();
    }
    if (tag != NULL) {
        tag->InsertBBLOperation(PinInstrument::GetCurrentBBL(), accessType);
    }

    CACHE_LEVEL::addmemcost(hit, this);

    _access[accessType][hit]++;

    return hit;
}

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
    ASSERTX(!INIErrorMsg(reader.ParseError(), filename, std::cerr));
    for (UINT32 i = 0; i < MAX_LEVEL; i++) {
        std::string name = _name[i];
        UINT32 linesize = reader.GetInteger(name, "linesize", -1);
        UINT32 cachesize = reader.GetInteger(name, "cachesize", -1);
        UINT32 associativity = reader.GetInteger(name, "associativity", -1);
        UINT32 allocation = reader.GetInteger(name, "allocation", -1);
        std::string policy = reader.Get(name, "policy", "");

        COST hitcost[MAX_COST_SITE];
        for (UINT32 j = 0; j < MAX_COST_SITE; j++) {
            COST cost = reader.GetReal(name, CostSiteName[j] + "hitcost", -1);
            if (cost >= 0) {
                hitcost[j] = cost;
            }
            else {
                hitcost[j] = 0;
            }
        }
        _cache[i] = new CACHE_LEVEL(name, policy, cachesize, linesize, associativity, allocation, hitcost);
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

VOID CACHE::Ul2Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    // second level unified cache
    const BOOL ul2Hit = _cache[UL2]->Access(addr, size, accessType);

    // third level unified cache
    if (! ul2Hit)
        _cache[UL3]->Access(addr, size, accessType);
}

VOID CACHE::InsRef(ADDRINT addr)
{
    const UINT32 size = 1; // assuming access does not cross cache lines
    const ACCESS_TYPE accessType = ACCESS_TYPE_LOAD;

    // ITLB
    _cache[ITLB]->AccessSingleLine(addr, accessType);

    // first level I-cache
    const BOOL il1Hit = _cache[IL1]->AccessSingleLine(addr, accessType);

    // second level unified Cache
    if ( ! il1Hit) Ul2Access(addr, size, accessType);
}


VOID CACHE::MemRefMulti(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    // DTLB
    _cache[DTLB]->AccessSingleLine(addr, ACCESS_TYPE_LOAD);

    // first level D-cache
    const BOOL dl1Hit = _cache[DL1]->Access(addr, size, accessType);

    // second level unified Cache
    if ( ! dl1Hit) Ul2Access(addr, size, accessType);
}

VOID CACHE::MemRefSingle(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    // DTLB
    _cache[DTLB]->AccessSingleLine(addr, ACCESS_TYPE_LOAD);

    // first level D-cache
    const BOOL dl1Hit = _cache[DL1]->AccessSingleLine(addr, accessType);

    // second level unified Cache
    if ( ! dl1Hit) Ul2Access(addr, size, accessType);
}

