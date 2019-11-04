//===- Cache.cpp - Cache implementation -------------------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "Storage.h"

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
/* Base class for storage level */
/* ===================================================================== */

STORAGE_LEVEL_BASE::STORAGE_LEVEL_BASE(STORAGE *storage, CostSite cost_site, StorageLevel storage_level)
  : _storage(storage),
    _cost_site(cost_site),
    _storage_level(storage_level)
{
    for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
    {
        _access[accessType][false] = 0;
        _access[accessType][true] = 0;
    }
}

// the current data reuse segment of each tag is stored as a separate unordered map,
// independent of which cache level this tag is in
VOID STORAGE_LEVEL_BASE::InsertOnHit(ADDRINT tag, ACCESS_TYPE accessType, BBLID bblid) {
    if (bblid != GLOBALBBLID) {
        DataReuseSegment &seg = _storage->_cost_package->_tag_seg_map[tag];
        seg.insert(bblid);
        // split then insert on store
        if (accessType == ACCESS_TYPE::ACCESS_TYPE_STORE) {
            _storage->_cost_package->_data_reuse.UpdateTrie(_storage->_cost_package->_data_reuse.getRoot(), seg);
            seg.clear();
            seg.insert(bblid);
        }
    }
}

VOID STORAGE_LEVEL_BASE::SplitOnMiss(ADDRINT tag) {
    DataReuseSegment &seg = _storage->_cost_package->_tag_seg_map[tag];
    _storage->_cost_package->_data_reuse.UpdateTrie(_storage->_cost_package->_data_reuse.getRoot(), seg);
    seg.clear();
}


std::ostream & STORAGE_LEVEL_BASE::StatsLong(std::ostream & out) const
{
    const UINT32 headerWidth = 19;
    const UINT32 numberWidth = 10;

    out << Name() << ":" << std::endl;

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

    return out;
}


/* ===================================================================== */
/* Cache level */
/* ===================================================================== */

CACHE_LEVEL::CACHE_LEVEL(STORAGE *storage, CostSite cost_site, StorageLevel storage_level, std::string policy, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, UINT32 allocation, COST hitcost[MAX_COST_SITE])
  : STORAGE_LEVEL_BASE(storage, cost_site, storage_level),
    _cacheSize(cacheSize),
    _lineSize(lineSize),
    _associativity(associativity),
    _replacement_policy(policy),
    STORE_ALLOCATION(allocation),
    _lineShift(FloorLog2(lineSize)),
    _setIndexMask((cacheSize / (associativity * lineSize)) - 1)
{
    ASSERTX(IsPower2(_lineSize));
    ASSERTX(IsPower2(_setIndexMask + 1));
    // NumSets = cacheSize / (associativity * lineSize)
    if (policy == "direct_mapped") {
        for (UINT32 i = 0; i < NumSets(); i++) {
            DIRECT_MAPPED *_set = new DIRECT_MAPPED(storage, associativity);
            _set->SetAssociativity(associativity);
            _sets.push_back(_set);
        }
    }
    else if (policy == "round_robin") {
        for (UINT32 i = 0; i < NumSets(); i++) {
            ROUND_ROBIN *_set = new ROUND_ROBIN(storage, associativity);
            _set->SetAssociativity(associativity);
            _sets.push_back(_set);
        }
    }
    else if (policy == "lru") {
        for (UINT32 i = 0; i < NumSets(); i++) {
            LRU *_set = new LRU(storage, associativity);
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


VOID CACHE_LEVEL::AddMemCost(BBLID bblid)
{
    // When this is a CPU cache level, for example,
    // _hitcost[PIM] will be assigned to 0
    if (bblid != GLOBALBBLID) {
        for (UINT32 i = 0; i < MAX_COST_SITE; i++)
            _storage->_cost_package->_bbl_memory_cost[i][bblid] += _hitcost[i];
    }
} 


BOOL CACHE_LEVEL::Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType, BBLID bblid)
{
    const ADDRINT highAddr = addr + size;
    BOOL allHit = true;

    const ADDRINT lineSize = LineSize();
    const ADDRINT notLineMask = ~(lineSize - 1);
    do
    {
        allHit &= AccessSingleLine(addr, accessType, bblid);
        addr = (addr & notLineMask) + lineSize; // start of next cache line

    } while (addr < highAddr);

    return allHit;
}


BOOL CACHE_LEVEL::AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType, BBLID bblid)
{
    ADDRINT tagaddr;
    UINT32 setIndex;

    SplitAddress(addr, tagaddr, setIndex);

    CACHE_SET *set = _sets[setIndex];

    CACHE_TAG *tag = set->Find(tagaddr);
    BOOL hit = (tag != NULL);

    AddMemCost(bblid);

    _access[accessType][hit]++;

    // On miss: Loads always allocate, stores optionally
    // 1. Replace the current level with the demanding tag
    // 2. Go and access next level
    // This is the implementation of an inclusive cache,
    // every access to a memory address will promote that address to L1
    if ((!hit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
    {
        tag = set->Replace(tagaddr);

        ASSERTX(_next_level != NULL);
        // We only keep track of the data reuse chain from the view of CPU
        // this is conservative as it may lead to larger reuse cost than actual
        // _hitcost[CPU] > 0 means that this is a CPU cache level
        if (_next_level->_storage_level == MEM && _hitcost[CPU] > 0)
            SplitOnMiss(tagaddr);
        _next_level->AccessSingleLine(addr, accessType, bblid);
    }
    if (hit && _hitcost[CPU] > 0) {
        InsertOnHit(tagaddr, accessType, bblid);
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

std::ostream & CACHE_LEVEL::StatsLong(std::ostream &out) const
{
    const UINT32 headerWidth = 19;
    const UINT32 numberWidth = 10;
    STORAGE_LEVEL_BASE::StatsLong(out);
    out << StringString("Flushes:         ", headerWidth, ' ')
        << StringInt(Flushes(), numberWidth) << std::endl;
    out << StringString("Stat Resets:     ", headerWidth, ' ')
        << StringInt(Resets(), numberWidth) << std::endl;
    return out;
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
MEMORY_LEVEL::MEMORY_LEVEL(STORAGE *storage, CostSite cost_site, StorageLevel storage_level, COST hitcost[MAX_COST_SITE])
  : STORAGE_LEVEL_BASE(storage, cost_site, storage_level)
{
    for (UINT32 i = 0; i < MAX_COST_SITE; i++)
        _hitcost[i] = hitcost[i];
}

VOID MEMORY_LEVEL::AddMemCost(BBLID bblid)
{
    if (bblid != GLOBALBBLID) {
        // increase counter of cache miss
        _storage->_cost_package->_cache_miss[bblid]++;
        for (UINT32 i = 0; i < MAX_COST_SITE; i++)
            _storage->_cost_package->_bbl_memory_cost[i][bblid] += _hitcost[i];
    }
}

BOOL MEMORY_LEVEL::Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType, BBLID bblid)
{
    // TODO: Implement this later
    const ADDRINT highAddr = addr + size;
    BOOL allHit = true;

    const ADDRINT lineSize = 64;
    const ADDRINT notLineMask = ~(lineSize - 1);
    do
    {
        allHit &= AccessSingleLine(addr, accessType, bblid);
        addr = (addr & notLineMask) + lineSize; // start of next cache line

    } while (addr < highAddr);

    return allHit;
}


BOOL MEMORY_LEVEL::AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType, BBLID bblid)
{
    // always hit memory
    AddMemCost(bblid);
    _access[accessType][true]++;
    return true;
}


/* ===================================================================== */
/* Storage */
/* ===================================================================== */

STORAGE::STORAGE()
{
    memset(_storage, 0, sizeof(_storage));
}

STORAGE::~STORAGE()
{
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < MAX_LEVEL; j++) {
            if (_storage[i][j] != NULL)
                delete _storage[i][j];
        }
    }
}

void STORAGE::initialize(CostPackage *cost_package, ConfigReader &reader)
{
    _cost_package = cost_package;
    ReadConfig(reader);
}

void STORAGE::ReadConfig(ConfigReader &reader)
{
    INT32 global_linesize = -1;
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        INT32 last_level = -1;
        // read cache configuration and create cache hierarchy
        for (UINT32 j = 0; j < MAX_LEVEL - 1; j++) {
            std::string name = CostSiteName[i] + "/" + StorageLevelName[j];
            auto sections = reader.Sections();
            if (sections.find(name) == sections.end()) {
                break;
            }
            last_level = j;

            std::string attributes[5] = {"linesize", "cachesize", "associativity", "allocation", "policy"};
            bool readerror[5] = {0};

            INT32 linesize = reader.GetInteger(name, attributes[0], -1);
            readerror[0] = (linesize == -1);
            INT32 cachesize = reader.GetInteger(name, attributes[1], -1);
            readerror[1] = (cachesize == -1);
            INT32 associativity = reader.GetInteger(name, attributes[2], -1);
            readerror[2] = (associativity == -1);
            INT32 allocation = reader.GetInteger(name, attributes[3], -1);
            readerror[3] = (allocation == -1);
            std::string policy = reader.Get(name, attributes[4], "");
            readerror[4] = (policy == "");

            for (UINT32 i = 0; i < 5; i++) {
                if (readerror[i]) {
                    errormsg() << "Cache: Invalid attribute `" << attributes[i] << "` in cache level `" << name <<"`" << std::endl;
                    ASSERTX(0);
                }
            }

            if (global_linesize == -1) {
                global_linesize = linesize;
            }
            else {
                if (linesize != global_linesize) {
                    errormsg() << "Cache: Line size of cache levels are not consistent" << std::endl;
                    ASSERTX(0);
                }
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
            _storage[i][j] = new CACHE_LEVEL(this, (CostSite)i, (StorageLevel)j, policy, cachesize, linesize, associativity, allocation, hitcost);
            // _storage[i][0:j] are not NULL for sure.
            if (j == UL2) {
                _storage[i][IL1]->_next_level = _storage[i][j];
                _storage[i][DL1]->_next_level = _storage[i][j];
            }
            if (j == UL3) {
                _storage[i][UL2]->_next_level = _storage[i][j];
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
        _storage[i][MEM] = new MEMORY_LEVEL(this, (CostSite)i, MEM, hitcost);
        if (last_level == DL1) {
            _storage[i][IL1]->_next_level = _storage[i][MEM];
            _storage[i][DL1]->_next_level = _storage[i][MEM];
            _storage_top[i][IL1] = _storage[i][IL1];
            _storage_top[i][DL1] = _storage[i][DL1];
        }
        else if (last_level == IL1) {
            _storage_top[i][IL1] = _storage[i][IL1];
            _storage_top[i][DL1] = _storage[i][MEM];
        }
        else if (last_level != -1) {
            _storage[i][last_level]->_next_level = _storage[i][MEM];
            _storage_top[i][IL1] = _storage[i][IL1];
            _storage_top[i][DL1] = _storage[i][DL1];
        }
        else {
            _storage_top[i][IL1] = _storage[i][MEM];
            _storage_top[i][DL1] = _storage[i][MEM];
        }
    }
}

std::ostream& STORAGE::WriteConfig(std::ostream& out)
{
    // TODO
    return out;
}

void STORAGE::WriteConfig(const std::string filename)
{
    std::ofstream out;
    out.open(filename.c_str(), std::ios_base::out);
    WriteConfig(out);
    out.close();
}

std::ostream& STORAGE::WriteStats(std::ostream& out)
{
    // TODO: ignore this for now
    // for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
    //     for (UINT32 j = 0; j < MAX_LEVEL; j++) {
    //         _storage[i][j]->StatsLong(out);
    //     }
    // }
    return out;
}
void STORAGE::WriteStats(const std::string filename)
{
    std::ofstream out;
    out.open(filename.c_str(), std::ios_base::out);
    WriteStats(out);
    out.close();
}

VOID STORAGE::InstrCacheRef(ADDRINT addr, BBLID bblid)
{
    // TODO: We do not consider TLB cost for now.
    // _storage[ITLB]->AccessSingleLine(addr, accessType);

    // assuming instruction cache access does not cross cache line
    // first level I-cache
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        _storage_top[i][IL1]->AccessSingleLine(addr, ACCESS_TYPE_LOAD, bblid);
    }
}


VOID STORAGE::DataCacheRefMulti(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType, BBLID bblid)
{
    // TODO: We do not consider TLB cost for now.
    // _storage[DTLB]->AccessSingleLine(addr, ACCESS_TYPE_LOAD);

    // first level D-cache
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        _storage_top[i][DL1]->Access(addr, size, accessType, bblid);
    }
}

VOID STORAGE::DataCacheRefSingle(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType, BBLID bblid)
{
    // TODO: We do not consider TLB cost for now.
    // _storage[DTLB]->AccessSingleLine(addr, ACCESS_TYPE_LOAD);

    // first level D-cache
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        _storage_top[i][DL1]->Access(addr, size, accessType, bblid);
    }
}

