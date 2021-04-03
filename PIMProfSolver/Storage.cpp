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


std::string StringInt(uint64_t val, uint32_t width=0, CHAR padding=' ')
{
    std::ostringstream ostr;
    ostr.setf(std::ios::fixed,std::ios::floatfield);
    ostr.fill(padding);
    ostr << std::setw(width) << val;
    return ostr.str();
}

std::string StringHex(uint64_t val, uint32_t width=0, CHAR padding=' ')
{
    std::ostringstream ostr;
    ostr.setf(std::ios::fixed,std::ios::floatfield);
    ostr.fill(padding);
    ostr << std::setw(width) << std::hex << "0x" << val;
    return ostr.str();
}

std::string StringString(std::string val, uint32_t width=0, CHAR padding=' ')
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
    for (uint32_t accessType = 0; accessType < MAX_ACCESS_TYPE; accessType++)
    {
        _access[accessType][false] = 0;
        _access[accessType][true] = 0;
    }
}

// the current data reuse segment of each tag is stored as a separate unordered map,
// independent of which cache level this tag is in
void STORAGE_LEVEL_BASE::InsertOnHit(ADDRINT tag, ACCESS_TYPE accessType, BBLID bblid) {
    if (bblid != GLOBALBBLID || _storage->_cost_package->_command_line_parser.enableglobalbbl()) {
        DataReuseSegment &seg = _storage->_cost_package->_tag_seg_map[tag];
        seg.insert(bblid);
        int32_t threadcount = _storage->_cost_package->_thread_count;
        if (threadcount > seg.getCount())
            seg.setCount(threadcount);
        // split then insert on store
        if (accessType == ACCESS_TYPE::ACCESS_TYPE_STORE) {
            _storage->_cost_package->_bbl_data_reuse.UpdateTrie(_storage->_cost_package->_bbl_data_reuse.getRoot(), seg);
            seg.clear();
            seg.insert(bblid);
            if (threadcount > seg.getCount())
                seg.setCount(threadcount);
        }
    }
}

void STORAGE_LEVEL_BASE::SplitOnMiss(ADDRINT tag) {
    DataReuseSegment &seg = _storage->_cost_package->_tag_seg_map[tag];
    _storage->_cost_package->_bbl_data_reuse.UpdateTrie(_storage->_cost_package->_bbl_data_reuse.getRoot(), seg);
    seg.clear();
}


std::ostream & STORAGE_LEVEL_BASE::StatsLong(std::ostream & out) const
{
    const uint32_t headerWidth = 19;
    const uint32_t numberWidth = 10;

    out << Name() << ":" << std::endl;

    for (uint32_t i = 0; i < MAX_ACCESS_TYPE; i++)
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

CACHE_LEVEL::CACHE_LEVEL(STORAGE *storage, CostSite cost_site, StorageLevel storage_level, std::string policy, uint32_t cacheSize, uint32_t lineSize, uint32_t associativity, uint32_t allocation, COST hitcost[MAX_COST_SITE])
  : STORAGE_LEVEL_BASE(storage, cost_site, storage_level),
    _cacheSize(cacheSize),
    _lineSize(lineSize),
    _associativity(associativity),
    _replacement_policy(policy),
    STORE_ALLOCATION(allocation),
    _lineShift(FloorLog2(lineSize)),
    _setIndexMask((cacheSize / (associativity * lineSize)) - 1)
{
    assert(IsPower2(_lineSize));
    assert(IsPower2(_setIndexMask + 1));
    // NumSets = cacheSize / (associativity * lineSize)
    if (policy == "direct_mapped") {
        for (uint32_t i = 0; i < NumSets(); i++) {
            DIRECT_MAPPED *_set = new DIRECT_MAPPED(storage, associativity);
            _set->SetAssociativity(associativity);
            _sets.push_back(_set);
        }
    }
    else if (policy == "round_robin") {
        for (uint32_t i = 0; i < NumSets(); i++) {
            ROUND_ROBIN *_set = new ROUND_ROBIN(storage, associativity);
            _set->SetAssociativity(associativity);
            _sets.push_back(_set);
        }
    }
    else if (policy == "lru") {
        for (uint32_t i = 0; i < NumSets(); i++) {
            LRU *_set = new LRU(storage, associativity);
            _set->SetAssociativity(associativity);
            _sets.push_back(_set);
        }
    }
    else {
        errormsg() << "Invalid cache replacement policy name!" << std::endl;
        assert(0);
    }
    for (uint32_t i = 0; i < MAX_COST_SITE; i++)
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

int bbl_costcount = 0;


void CACHE_LEVEL::AddMemCost(BBLID bblid, uint32_t simd_len)
{
    // When this is a CPU cache level, for example,
    // _hitcost[PIM] will be assigned to 0
    if (bblid != GLOBALBBLID || _storage->_cost_package->_command_line_parser.enableglobalbbl()) {
        // if this instruction itself is not a simd instruction
        // but the instruction is in an accelerable function or parallelizable region,
        // then this instruction is a normal instruction but parallelizable (simd_len = 1)
        if (simd_len == 0 && (_storage->_cost_package->_inAcceleratorFunction || _storage->_cost_package->_bbl_parallelizable[bblid])) {
            simd_len = 1;
        }
        for (int i = 0; i < MAX_COST_SITE; i++) {
            COST cost = _hitcost[i] * _storage->_cost_package->_thread_count;
            if (simd_len) {
                int multiplier = (_storage->_cost_package->_simd_capability[i]<simd_len ? simd_len/_storage->_cost_package->_simd_capability[i] : 1);
                cost = cost * multiplier / _storage->_cost_package->_core_count[i];
            }
            _storage->_cost_package->_bbl_memory_cost[i][bblid] += cost;
#ifdef PIMPROF_MPKI
            _storage->_cost_package->_bbl_storage_level_cost[i][_storage_level][bblid] += cost;
            if (bbl_costcount < 1000000 && i == PIM && _storage_level == MEM && cost != 0 && cost != 80) {
                std::cout << (simd_len?"T ":"F ") << _storage->_cost_package->_thread_count << " " << i << " " << _storage_level << " " << cost << std::endl;
                bbl_costcount++;
            }
#endif
        }
    }
}


bool CACHE_LEVEL::Access(ADDRINT addr, uint32_t size, ACCESS_TYPE accessType, BBLID bblid, uint32_t simd_len)
{
    const ADDRINT highAddr = addr + size;
    bool allHit = true;

    const ADDRINT lineSize = LineSize();
    const ADDRINT notLineMask = ~(lineSize - 1);
    do
    {
        allHit &= AccessSingleLine(addr, accessType, bblid, simd_len);
        addr = (addr & notLineMask) + lineSize; // start of next cache line

    } while (addr < highAddr);

    return allHit;
}

void CACHE_LEVEL::AddInstructionMemCost(BBLID bblid, uint32_t simd_len)
{
    if (bblid != GLOBALBBLID || _storage->_cost_package->_command_line_parser.enableglobalbbl()) {
        // if this instruction itself is not a simd instruction
        // but the instruction is in an accelerable function or parallelizable region,
        // then this instruction is a normal instruction but parallelizable (simd_len = 1)
        if (simd_len == 0 && (_storage->_cost_package->_inAcceleratorFunction || _storage->_cost_package->_bbl_parallelizable[bblid])) {
            simd_len = 1;
        }
        for (int i = 0; i < MAX_COST_SITE; i++) {
            COST cost = _hitcost[i] * _storage->_cost_package->_thread_count;
            if (simd_len) {
                int multiplier = (_storage->_cost_package->_simd_capability[i]<simd_len ? simd_len/_storage->_cost_package->_simd_capability[i] : 1);
                cost = cost * multiplier / _storage->_cost_package->_core_count[i];
            }
            _storage->_cost_package->_bbl_instruction_memory_cost[i][bblid] += cost;
        }
    }
}


bool CACHE_LEVEL::AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType, BBLID bblid, uint32_t simd_len)
{
    ADDRINT tagaddr;
    uint32_t setIndex;

    SplitAddress(addr, tagaddr, setIndex);

    CACHE_SET *set = _sets[setIndex];

    CACHE_TAG *tag = set->Find(tagaddr);
    bool hit = (tag != NULL);

    // Since the cost in config is the total access latency of hitting a cache level
    // we only increase the total cost when there is a hit.
    if (hit) {
        AddMemCost(bblid, simd_len);
        if (_storage_level == 0) {
            AddInstructionMemCost(bblid, simd_len);
        }
    }
    

    _access[accessType][hit]++;

    // On miss: Loads always allocate, stores optionally
    // 1. Replace the current level with the demanding tag
    // 2. Go and access next level
    // This is the implementation of an inclusive cache,
    // every access to a memory address will promote that address to L1
    if ((!hit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
    {
        tag = set->Replace(tagaddr);

        assert(_next_level != NULL);
        // We only keep track of the data reuse chain from the view of CPU
        // this is conservative as it may lead to larger reuse cost than actual
        // _hitcost[CPU] > 0 means that this is a CPU cache level
        if (_next_level->_storage_level == MEM && _hitcost[CPU] > 0)
            SplitOnMiss(tagaddr);
        _next_level->AccessSingleLine(addr, accessType, bblid, simd_len);
    }
    if (hit && _hitcost[CPU] > 0) {
        InsertOnHit(tagaddr, accessType, bblid);
    }

    return hit;
}

void CACHE_LEVEL::Flush()
{
    for (int32_t index = NumSets(); index >= 0; index--)
    {
        CACHE_SET *set = _sets[index];
        set->Flush();
    }
    IncFlushCounter();
}

std::ostream & CACHE_LEVEL::StatsLong(std::ostream &out) const
{
    const uint32_t headerWidth = 19;
    const uint32_t numberWidth = 10;
    STORAGE_LEVEL_BASE::StatsLong(out);
    out << StringString("Flushes:         ", headerWidth, ' ')
        << StringInt(Flushes(), numberWidth) << std::endl;
    out << StringString("Stat Resets:     ", headerWidth, ' ')
        << StringInt(Resets(), numberWidth) << std::endl;
    return out;
}

void CACHE_LEVEL::ResetStats()
{
    for (uint32_t accessType = 0; accessType < MAX_ACCESS_TYPE; accessType++)
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
    for (uint32_t i = 0; i < MAX_COST_SITE; i++)
        _hitcost[i] = hitcost[i];
}

void MEMORY_LEVEL::AddMemCost(BBLID bblid, uint32_t simd_len)
{
    if (bblid != GLOBALBBLID || _storage->_cost_package->_command_line_parser.enableglobalbbl()) {
#ifdef PIMPROF_MPKI
        // increase counter of cache miss
        _storage->_cost_package->_cache_miss[bblid]++;
#endif
        // if this instruction itself is not a simd instruction
        // but the instruction is in an accelerable function or parallelizable region,
        // then this instruction is a normal instruction but parallelizable (simd_len = 1)
        if (simd_len == 0 && (_storage->_cost_package->_inAcceleratorFunction || _storage->_cost_package->_bbl_parallelizable[bblid])) {
            simd_len = 1;
        }
        for (int i = 0; i < MAX_COST_SITE; i++) {
            COST cost = _hitcost[i] * _storage->_cost_package->_thread_count;
            if (simd_len) {
                int multiplier = (_storage->_cost_package->_simd_capability[i]<simd_len ? simd_len/_storage->_cost_package->_simd_capability[i] : 1);
                cost = cost * multiplier / _storage->_cost_package->_core_count[i];
            }
            _storage->_cost_package->_bbl_memory_cost[i][bblid] += cost;
#ifdef PIMPROF_MPKI
            _storage->_cost_package->_bbl_storage_level_cost[i][_storage_level][bblid] += cost;
            // if (bbl_costcount < 1000000 && i == PIM && _storage_level == MEM && cost != 0 && cost != 80) {
            //     std::cout << (simd_len?"T ":"F ") << _storage->_cost_package->_thread_count << " " << i << " " << _storage_level << " " << cost << std::endl;
            //     bbl_costcount++;
            // }
#endif
        }
    }
}

bool MEMORY_LEVEL::Access(ADDRINT addr, uint32_t size, ACCESS_TYPE accessType, BBLID bblid, uint32_t simd_len)
{
    // TODO: Implement this later
    const ADDRINT highAddr = addr + size;
    bool allHit = true;

    const ADDRINT lineSize = 64;
    const ADDRINT notLineMask = ~(lineSize - 1);
    do
    {
        allHit &= AccessSingleLine(addr, accessType, bblid, simd_len);
        addr = (addr & notLineMask) + lineSize; // start of next cache line

    } while (addr < highAddr);

    return allHit;
}


bool MEMORY_LEVEL::AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType, BBLID bblid, uint32_t simd_len)
{
    // always hit memory
    AddMemCost(bblid, simd_len);
    _access[accessType][true]++;
    return true;
}


/* ===================================================================== */
/* Storage */
/* ===================================================================== */

STORAGE::STORAGE()
{
    memset(_storage, 0, sizeof(_storage));
    memset(_last_icacheline, 0, sizeof(_last_icacheline));
}

STORAGE::~STORAGE()
{
    for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
        for (uint32_t j = 0; j < MAX_LEVEL; j++) {
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
    int32_t global_linesize = -1;
    for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
        int32_t last_level = -1;
        // read cache configuration and create cache hierarchy
        for (uint32_t j = 0; j < MAX_LEVEL - 1; j++) {
            std::string name = CostSiteName[i] + "/" + StorageLevelName[j];
            auto sections = reader.Sections();
            if (sections.find(name) == sections.end()) {
                break;
            }
            last_level = j;

            std::string attributes[5] = {"linesize", "cachesize", "associativity", "allocation", "policy"};
            bool readerror[5] = {0};

            int32_t linesize = reader.GetInteger(name, attributes[0], -1);
            readerror[0] = (linesize == -1);
            int32_t cachesize = reader.GetInteger(name, attributes[1], -1);
            readerror[1] = (cachesize == -1);
            int32_t associativity = reader.GetInteger(name, attributes[2], -1);
            readerror[2] = (associativity == -1);
            int32_t allocation = reader.GetInteger(name, attributes[3], -1);
            readerror[3] = (allocation == -1);
            std::string policy = reader.Get(name, attributes[4], "");
            readerror[4] = (policy == "");

            for (uint32_t i = 0; i < 5; i++) {
                if (readerror[i]) {
                    errormsg() << "Cache: Invalid attribute `" << attributes[i] << "` in cache level `" << name <<"`" << std::endl;
                    assert(0);
                }
            }

            if (global_linesize == -1) {
                global_linesize = linesize;
            }
            else {
                if (linesize != global_linesize) {
                    errormsg() << "Cache: Line size of cache levels are not consistent" << std::endl;
                    assert(0);
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
                assert(0);
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
            assert(0);
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
    for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
        for (uint32_t j = 0; j < MAX_LEVEL; j++) {
            if (_storage[i][j] != NULL) {
                _storage[i][j]->StatsLong(out);
                out << std::endl << std::endl;
            }
        }
    }
    return out;
}
void STORAGE::WriteStats(const std::string filename)
{
    std::ofstream out;
    out.open(filename.c_str(), std::ios_base::out);
    WriteStats(out);
    out.close();
}

void STORAGE::InstrCacheRef(ADDRINT addr, uint32_t size, BBLID bblid, uint32_t simd_len)
{
    // TODO: We do not consider TLB cost for now.
    // _storage[ITLB]->AccessSingleLine(addr, accessType);

    // first level I-cache
    // assuming the core reads full instruction cache lines and caches them internally for subsequent instructions, similar assumption as Sniper and ZSim.
    for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
        const ADDRINT lineSize = static_cast<CACHE_LEVEL *>(_storage_top[i][IL1])->LineSize();
        const ADDRINT notLineMask = ~(lineSize - 1);
        ADDRINT curLine = addr & notLineMask;
        ADDRINT nextLine = curLine + lineSize;
        // if accessing same cache line as the one previously accessed
        if (curLine == _last_icacheline[i]) {
            if (addr + size >= nextLine) {
                _storage_top[i][IL1]->AccessSingleLine(nextLine, ACCESS_TYPE_LOAD, bblid, simd_len);
                _last_icacheline[i] = nextLine;

#ifdef PIMPROFTRACE
                if (i == 0) {
                    (*_cost_package->_trace_file[0])
                    << std::hex << "[0] I, 0x"
                    << nextLine << " " << std::dec
                    << 64 << std::endl;
                }
#endif
            }
            // otherwise do nothing
        }
        else {
            if (addr + size >= nextLine) {
                _storage_top[i][IL1]->AccessSingleLine(curLine, ACCESS_TYPE_LOAD, bblid, simd_len);
                _storage_top[i][IL1]->AccessSingleLine(nextLine, ACCESS_TYPE_LOAD, bblid, simd_len);
                _last_icacheline[i] = nextLine;

#ifdef PIMPROFTRACE
                if (i == 0) {
                    (*_cost_package->_trace_file[0])
                    << std::hex << "[0] I, 0x"
                    << curLine << " " << std::dec
                    << 64 << std::endl;
                    (*_cost_package->_trace_file[0])
                    << std::hex << "[0] I, 0x"
                    << nextLine << " " << std::dec
                    << 64 << std::endl;
                }
#endif
            }
            else {
                _storage_top[i][IL1]->AccessSingleLine(curLine, ACCESS_TYPE_LOAD, bblid, simd_len);
                _last_icacheline[i] = curLine;

#ifdef PIMPROFTRACE
                if (i == 0) {
                    (*_cost_package->_trace_file[0])
                    << std::hex << "[0] I, 0x"
                    << curLine << " " << std::dec
                    << 64 << std::endl;
                }
#endif
            }
        }
    }
}


void STORAGE::DataCacheRef(ADDRINT ip, ADDRINT addr, uint32_t size, ACCESS_TYPE accessType, BBLID bblid, uint32_t simd_len)
{
    // TODO: We do not consider TLB cost for now.
    // _storage[DTLB]->AccessSingleLine(addr, ACCESS_TYPE_LOAD);

    // first level D-cache
    for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
        _storage_top[i][DL1]->Access(addr, size, accessType, bblid, simd_len);
    }
#ifdef PIMPROFTRACE
    (*_cost_package->_trace_file[0])
    << "[0] "
    << (accessType == ACCESS_TYPE_LOAD ? "R, " : "W, ")
    << std::hex << "0x" << addr << std::dec << " "
    << size
    << std::hex << " (0x" << ip << ")" << std::dec
    << std::endl;
#endif
}

