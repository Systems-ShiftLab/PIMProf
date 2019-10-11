//===- Cache.cpp - Cache implementation -------------------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "Cache.h"

using namespace PIMProf;

/* ===================================================================== */
/* Cache */
/* ===================================================================== */


std::string StringInt(UINT64 val, UINT32 width=0, CHAR padding=' ')
{
    std::ostringstream ostr;
    ostr.setf(std::ios::fixed,std::ios::floatfield);
    ostr.fill(padding);
    ostr << std::setw(width) << val;
    return ostr.str();
}

std::string StringHex(UINT64 val, UINT32 width=0, CHAR padding=' ')
{
    std::ostringstream ostr;
    ostr.setf(std::ios::fixed,std::ios::floatfield);
    ostr.fill(padding);
    ostr << std::setw(width) << std::hex << "0x" << val;
    return ostr.str();
}

std::string StringString(std::string val, UINT32 width=0, CHAR padding=' ')
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

VOID CACHE_TAG::InsertOnHit(BBLID bblid, ACCESS_TYPE accessType) {
    if (bblid != GLOBALBBLID) {
        _seg.insert(bblid);
        // split then insert on store
        if (accessType == ACCESS_TYPE::ACCESS_TYPE_STORE) {
            _cache->_cost_package->_data_reuse.UpdateTrie(_cache->_cost_package->_data_reuse.getRoot(), _seg);
            _seg.clear();
            _seg.insert(bblid);
        }
    }
}

VOID CACHE_TAG::SplitOnMiss() {
    _cache->_cost_package->_data_reuse.UpdateTrie(_cache->_cost_package->_data_reuse.getRoot(), _seg);
    _seg.clear();
}

/* ===================================================================== */
/* Base class for cache level */
/* ===================================================================== */

STORAGE_LEVEL_BASE::STORAGE_LEVEL_BASE(CACHE *cache, std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity)
  : _cache(cache),
    _name(name),
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


std::ostream & STORAGE_LEVEL_BASE::StatsLong(std::ostream & out) const
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

CACHE_LEVEL::CACHE_LEVEL(CACHE *cache, std::string name, std::string policy, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, UINT32 allocation, COST hitcost[MAX_COST_SITE])
  : STORAGE_LEVEL_BASE(cache, name, cacheSize, lineSize, associativity),
    _replacement_policy(policy),
    STORE_ALLOCATION(allocation)
{
    // NumSets = cacheSize / (associativity * lineSize)
    if (policy == "direct_mapped") {
        for (UINT32 i = 0; i < NumSets(); i++) {
            DIRECT_MAPPED *_set = new DIRECT_MAPPED(cache, associativity);
            _set->SetAssociativity(associativity);
            _sets.push_back(_set);
        }
    }
    else if (policy == "round_robin") {
        for (UINT32 i = 0; i < NumSets(); i++) {
            ROUND_ROBIN *_set = new ROUND_ROBIN(cache, associativity);
            _set->SetAssociativity(associativity);
            _sets.push_back(_set);
        }
    }
    else if (policy == "lru") {
        for (UINT32 i = 0; i < NumSets(); i++) {
            LRU *_set = new LRU(cache, associativity);
            _set->SetAssociativity(associativity);
            _sets.push_back(_set);
        }
    }
    else {
        errormsg() << "Invalid cache replacement policy name!" << std::endl;
        ASSERTX(0);
    }
    for (UINT32 i = 0; i < MAX_COST_SITE; i++)
        _hitcost[i] = hitcost[i];
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


VOID CACHE_LEVEL::AddMemCost()
{
    BBLID bblid = _cache->_cost_package->_bbl_scope.top();
    if (bblid != GLOBALBBLID) {
        for (UINT32 i = 0; i < MAX_COST_SITE; i++)
            _cache->_cost_package->_BBL_memory_cost[i][bblid] += _hitcost[i];
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
        allHit &= AccessSingleLine(addr, accessType);
        addr = (addr & notLineMask) + lineSize; // start of next cache line

    } while (addr < highAddr);

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

    AddMemCost();

    _access[accessType][hit]++;

    // On miss: Loads always allocate, stores optionally
    // 1. Replace the current level with the demanding tag
    // 2. Go and access next level
    if ((!hit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
    {
        tag = set->Replace(tagaddr);
        // tag->SplitOnMiss();
        ASSERTX(_next_level != NULL);
        _next_level->AccessSingleLine(addr, accessType);
    }
    if (hit) {
        // tag->InsertOnHit(_cache->_cost_package->_bbl_scope.top(), accessType);
    }

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
/* Memory Level */
/* ===================================================================== */
MEMORY_LEVEL::MEMORY_LEVEL(CACHE *cache, std::string name, COST hitcost[MAX_COST_SITE])
  : STORAGE_LEVEL_BASE(cache, name, 64, 64, 1) // make the constructor happy
{
    for (UINT32 i = 0; i < MAX_COST_SITE; i++)
        _hitcost[i] = hitcost[i];
}

VOID MEMORY_LEVEL::AddMemCost()
{
    BBLID bblid = _cache->_cost_package->_bbl_scope.top();
    if (bblid != GLOBALBBLID) {
        for (UINT32 i = 0; i < MAX_COST_SITE; i++)
            _cache->_cost_package->_BBL_memory_cost[i][bblid] += _hitcost[i];
    }
}

BOOL MEMORY_LEVEL::Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    const ADDRINT highAddr = addr + size;
    BOOL allHit = true;

    const ADDRINT lineSize = LineSize();
    const ADDRINT notLineMask = ~(lineSize - 1);
    do
    {
        allHit &= AccessSingleLine(addr, accessType);
        addr = (addr & notLineMask) + lineSize; // start of next cache line

    } while (addr < highAddr);

    return allHit;
}


BOOL MEMORY_LEVEL::AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType)
{
    // always hit memory
    AddMemCost();
    _access[accessType][true]++;
    return true;
}


/* ===================================================================== */
/* Cache */
/* ===================================================================== */

CACHE::CACHE()
{
    memset(_cache, 0, sizeof(_cache));
}

CACHE::~CACHE()
{
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < MAX_LEVEL; j++)
            delete _cache[i][j];
    }
}

void CACHE::initialize(CostPackage *cost_package, ConfigReader &reader)
{
    _cost_package = cost_package;
    ReadConfig(reader);
}

void CACHE::ReadConfig(ConfigReader &reader)
{
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        INT32 last_level = -1;
        // read cache configuration and create cache hierarchy
        for (UINT32 j = 0; j < MAX_LEVEL - 1; j++) {
            std::string name = CostSiteName[i] + "/" + StorageLevelName[j];
            auto sections = reader.Sections();
            if (sections.find(name) == sections.end()) {
                break;
            }
            last_level = j - 1;

            bool readerror = false;
            INT32 linesize = reader.GetInteger(name, "linesize", -1);
            readerror |= (linesize == -1);
            INT32 cachesize = reader.GetInteger(name, "cachesize", -1);
            readerror |= (cachesize == -1);
            INT32 associativity = reader.GetInteger(name, "associativity", -1);
            readerror |= (associativity == -1);
            INT32 allocation = reader.GetInteger(name, "allocation", -1);
            readerror |= (allocation == -1);
            std::string policy = reader.Get(name, "policy", "");
            readerror |= (policy == "");

            if (readerror) {
                errormsg() << "Cache: Invalid attribute in cache level `" << name <<"`" << std::endl;
                ASSERTX(0);
            }

            COST hitcost[MAX_COST_SITE];
            memset(hitcost, 0, sizeof(hitcost));
            COST cost = reader.GetReal(name, "hitcost", -1);
            if (cost >= 0) {
                hitcost[i] = cost;
            }
            else {
                errormsg() << "Cache: Invalid hitcost in cache level `" << name <<"`" << std::endl;
                ASSERTX(0);
            }
            _cache[i][j] = new CACHE_LEVEL(this, name, policy, cachesize, linesize, associativity, allocation, hitcost);
            // _cache[i][0:j] are not NULL for sure.
            if (j == UL2) {
                _cache[i][IL1]->_next_level = _cache[i][j];
                _cache[i][DL1]->_next_level = _cache[i][j];
            }
            if (j == UL3) {
                _cache[i][UL2]->_next_level = _cache[i][j];
            }
        }

        // connect memory
        std::string name = CostSiteName[i] + "/" + StorageLevelName[MEM];
        COST cost = reader.GetReal(name, "hitcost", -1);
        COST hitcost[MAX_COST_SITE];
        memset(hitcost, 0, sizeof(hitcost));
        if (cost >= 0) {
            hitcost[i] = cost;
        }
        else {
            errormsg() << "Memory: Invalid hitcost in memory." << std::endl;
            ASSERTX(0);
        }
        _cache[i][MEM] = new MEMORY_LEVEL(this, name, hitcost);
        if (last_level == DL1) {
            _cache[i][IL1]->_next_level = _cache[i][MEM];
            _cache[i][DL1]->_next_level = _cache[i][MEM];
        }
        else if (last_level != -1) {
            _cache[i][last_level]->_next_level = _cache[i][MEM];
        }
    }
}

std::ostream& CACHE::WriteConfig(std::ostream& out)
{
    // TODO
    return out;
}

void CACHE::WriteConfig(const std::string filename)
{
    std::ofstream out;
    out.open(filename.c_str(), ios_base::out);
    WriteConfig(out);
    out.close();
}

std::ostream& CACHE::WriteStats(std::ostream& out)
{
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < MAX_LEVEL; j++) {
            _cache[i][j]->StatsLong(out);
        }
    }
    return out;
}
void CACHE::WriteStats(const std::string filename)
{
    std::ofstream out;
    out.open(filename.c_str(), ios_base::out);
    WriteStats(out);
    out.close();
}

VOID CACHE::InstrCacheRef(ADDRINT addr)
{
    const ACCESS_TYPE accessType = ACCESS_TYPE_LOAD;

    // TODO: We do not consider TLB cost for now.
    // _cache[ITLB]->AccessSingleLine(addr, accessType);

    // assuming instruction cache access does not cross cache line
    // first level I-cache
    for (UINT32 i = 0; i < MAX_COST_SITE; i++)
        _cache[i][IL1]->AccessSingleLine(addr, accessType);
}


VOID CACHE::DataCacheRefMulti(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    // TODO: We do not consider TLB cost for now.
    // _cache[DTLB]->AccessSingleLine(addr, ACCESS_TYPE_LOAD);

    // first level D-cache
    for (UINT32 i = 0; i < MAX_COST_SITE; i++)
        _cache[i][DL1]->Access(addr, size, accessType);
}

VOID CACHE::DataCacheRefSingle(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    // TODO: We do not consider TLB cost for now.
    // _cache[DTLB]->AccessSingleLine(addr, ACCESS_TYPE_LOAD);

    // first level D-cache
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        
        _cache[i][DL1]->Access(addr, size, accessType);
    }
}

