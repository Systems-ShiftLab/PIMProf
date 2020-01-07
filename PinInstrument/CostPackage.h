//===- CostPackage.h - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

/* ===================================================================== */
/* CostPackage */
/* ===================================================================== */
#ifndef __COSTPACKAGE_H__
#define __COSTPACKAGE_H__

#include <stack>
#include <algorithm>
#include <iostream>
#include <unordered_map>

#include "pin.H"
#include "PinUtil.h"
#include "DataReuse.h"
#include "INIReader.h"

namespace PIMProf {
static const UINT32 MAX_INDEX = 4096;
static const UINT32 INDEX_SPECIAL = 3000;
static const UINT32 MAX_MEM_SIZE = 512;


class HashFunc
{
  public:
    // assuming UUID is already murmurhash-ed.
    std::size_t operator()(const UUID &key) const
    {
        size_t result = key.first ^ key.second;
        return result;
    }
};

class CostPackage {
  public:
    std::unordered_map<UUID, UINT32, HashFunc> _bbl_hash;
    BBLID _bbl_size = 0;
    bool _inAcceleratorFunction;
    /// whether the current function is in .omp function
    int _in_omp_parallel = 0;
    /// whether this bbl is in parallelizable region
    /// the value will be overwritten to true if in spawned thread
    std::vector<bool> _bbl_parallelizable;

  // multithread parameters
  public:
    /// protect everything in this region
    PIN_RWMUTEX _thread_count_rwmutex;
    /// thread count and the corresponding RW lock
    INT32 _thread_count = 0;
    /// indicate which BBL each thread is in now
    /// needs to acquire the write lock when pushing back a new BBLScope for the new thread
    std::vector<BBLScope> _thread_bbl_scope;
    std::vector<bool> _thread_in_roi;

#ifdef PIMPROFDEBUG
  public:
    UINT64 _total_instr_cnt = 0;
    UINT64 _total_simd_instr_cnt = 0;
    COST _total_simd_cost[MAX_COST_SITE] = {0};
    std::vector<UINT64> _bbl_visit_cnt;
    std::vector<UINT64> _bbl_instr_cnt;
    std::vector<UINT64> _simd_instr_cnt;
    std::vector<UINT64> _cache_miss;

    COST _type_instr_cnt[MAX_INDEX] = {0};
    COST _type_instr_cost[MAX_COST_SITE][MAX_INDEX] = {0};
#endif

  public:
    /// Construction of latency table follows the opcode generation function in
    /// $(PIN_ROOT)/source/tools/SimpleExamples/opcodemix.cpp
    COST _instruction_latency[MAX_COST_SITE][MAX_INDEX];
    /// the cost multiplier of each cost site
    COST _instruction_multiplier[MAX_COST_SITE];

    COST _ilp[MAX_COST_SITE];
    COST _mlp[MAX_COST_SITE];
    UINT32 _core_count[MAX_COST_SITE];
    UINT32 _simd_cost_multiplier[MAX_COST_SITE];

    /// the control latency when switching between sites
    COST _control_latency[MAX_COST_SITE][MAX_COST_SITE];

  public:
    /// the total instruction cost of each BB
    std::vector<COST> _bbl_instruction_cost[MAX_COST_SITE];
    /// the total memory cost of each BB
    std::vector<COST> _bbl_memory_cost[MAX_COST_SITE];

  public:
    /// keep track of the data reuse cost
    DataReuse _data_reuse;
    std::unordered_map<ADDRINT, DataReuseSegment> _tag_seg_map;

  public:
    void initialize();

    inline COST BBLInstructionCost(CostSite site, BBLID bbl) {
        if (_bbl_parallelizable[bbl]) {
            return _bbl_instruction_cost[site][bbl] * _instruction_multiplier[site] / _ilp[site] / _core_count[site];
        }
        else {
            return _bbl_instruction_cost[site][bbl] * _instruction_multiplier[site] / _ilp[site];
        }

    }
    inline COST BBLMemoryCost(CostSite site, BBLID bbl) {
        if (_bbl_parallelizable[bbl]) {
            return _bbl_memory_cost[site][bbl] / _mlp[site] / _core_count[site];
        }
        else {
            return _bbl_memory_cost[site][bbl] / _mlp[site];
        }
    }
};

} // namespace PIMProf

#endif // __COSTPACKAGE_H__
