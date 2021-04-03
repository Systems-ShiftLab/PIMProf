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
#include <memory>

#include "Util.h"
#include "DataReuse.h"
#include "INIReader.h"

namespace PIMProf {
static const uint32_t MAX_INDEX = 4096;
static const uint32_t INDEX_SPECIAL = 3000;
static const uint32_t MAX_MEM_SIZE = 512;

class CostPackage {
  public:
    ConfigReader _config_reader;

//   public:
//     UUIDHashMap<uint32_t> _bbl_hash;
//     BBLID _bbl_size = 0;
//     bool _inAcceleratorFunction;
//     /// whether the current function is in .omp function
//     int _in_omp_parallel = 0;
//     /// whether this bbl is in parallelizable region
//     /// the value will be overwritten to true if in spawned thread
//     std::vector<bool> _bbl_parallelizable;

//   // multithread parameters
//   public:
//     /// protect everything in this region
//     PIN_RWMUTEX _thread_count_rwmutex;
//     /// thread count and the corresponding RW lock
//     int32_t _thread_count = 0;
//     /// indicate which BBL each thread is in now
//     /// needs to acquire the write lock when pushing back a new BBLScope for the new thread
//     std::vector<BBLScope> _thread_bbl_scope;
//     std::vector<bool> _thread_in_roi;
//     std::vector<bool> _thread_in_roidecision;

//   public:
//     std::vector<int32_t> _previous_instr;
//     std::vector<std::ofstream *> _trace_file;

// #ifdef PIMPROF_MPKI
//   public:
//     uint64_t _total_instr_cnt = 0;
//     uint64_t _total_simd_instr_cnt = 0;
//     COST _total_simd_cost[MAX_COST_SITE] = {0};
//     std::vector<uint64_t> _bbl_visit_cnt;
//     std::vector<uint64_t> _bbl_instr_cnt;
//     std::vector<uint64_t> _simd_instr_cnt;
//     std::vector<uint64_t> _cache_miss;

//     COST _type_instr_cnt[MAX_INDEX] = {0};
//     COST _type_instr_cost[MAX_COST_SITE][MAX_INDEX] = {0};
// #endif

//     /// the number of site changes can only be known when
//     /// -roidecision is enabled, i.e., the site is known
//     uint64_t _enter_roi_cnt = 0;
//     uint64_t _exit_roi_cnt = 0;

//   public:
//     /// Construction of latency table follows the opcode generation function in
//     /// $(PIN_ROOT)/source/tools/SimpleExamples/opcodemix.cpp
//     COST _instruction_latency[MAX_COST_SITE][MAX_INDEX];
//     /// the cost multiplier of each cost site
//     COST _instruction_multiplier[MAX_COST_SITE];

//     COST _ilp[MAX_COST_SITE];
//     COST _mlp[MAX_COST_SITE];
//     uint32_t _core_count[MAX_COST_SITE];
//     uint32_t _simd_capability[MAX_COST_SITE];

//     /// the control latency when switching between sites
//     COST _control_latency[MAX_COST_SITE][MAX_COST_SITE];

//   public:
//     /// the total instruction cost of each BB
//     std::vector<COST> _bbl_instruction_cost[MAX_COST_SITE];
//     /// the total memory cost of each BB
//     std::vector<COST> _bbl_memory_cost[MAX_COST_SITE];
//     std::vector<CostSite> _roi_decision;

// #ifdef PIMPROF_MPKI
//     std::vector<COST> _bbl_storage_level_cost[MAX_COST_SITE][MAX_LEVEL];
//     std::vector<COST> _bbl_instruction_memory_cost[MAX_COST_SITE];
// #endif

//   public:
//     /// keep track of the data reuse cost
//     DataReuse _bbl_data_reuse;
//     std::unordered_map<ADDRINT, DataReuseSegment> _tag_seg_map;

  public:
    void initialize(int argc, char *argv[]);

//     void initializeNewBBL(UUID bblhash);

//     inline COST BBLInstructionCost(CostSite site, BBLID bbl) {
//         return _bbl_instruction_cost[site][bbl] * _instruction_multiplier[site] / _ilp[site];

//     }
//     inline COST BBLMemoryCost(CostSite site, BBLID bbl) {
//         return _bbl_memory_cost[site][bbl] / _mlp[site];
//     }
//     inline COST BBLInstructionMemoryCost(CostSite site, BBLID bbl) {
//         return _bbl_instruction_memory_cost[site][bbl] / _mlp[site];
//     }
// #ifdef PIMPROF_MPKI
//     inline COST BBLStorageLevelCost(CostSite site, StorageLevel lvl, BBLID bbl) {
//         return _bbl_storage_level_cost[site][lvl][bbl] / _mlp[site];
//     }
// #endif
};

} // namespace PIMProf

#endif // __COSTPACKAGE_H__
