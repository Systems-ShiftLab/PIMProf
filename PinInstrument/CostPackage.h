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

class CostPackage {
  public:
    // BBLScope information
    BBLScope _bbl_scope;
    BBLID _bbl_size;
    std::vector<bool> _inOpenMPRegion;

    long long int _instr_cnt;
    long long int _mem_instr_cnt;
    long long int _nonmem_instr_cnt;

    /// Construction of latency table follows the opcode generation function in
    /// $(PIN_ROOT)/source/tools/SimpleExamples/opcodemix.cpp
    COST _instruction_latency[MAX_COST_SITE][MAX_INDEX];
    /// the cost multiplier of each cost site
    COST _instruction_multiplier[MAX_COST_SITE];
    /// the total instruction cost of each BB
    std::vector<COST> _BBL_instruction_cost[MAX_COST_SITE];

    COST _ilp[MAX_COST_SITE];
    COST _mlp[MAX_COST_SITE];
    UINT32 _core_count[MAX_COST_SITE];


    /// the total memory cost of each BB
    std::vector<COST> _BBL_memory_cost[MAX_COST_SITE];

    /// the control latency when switching between sites
    COST _control_latency[MAX_COST_SITE][MAX_COST_SITE];


    DataReuse _data_reuse;

    std::unordered_map<ADDRINT, DataReuseSegment> _tag_seg_map;

  public:
    void initialize();

    inline COST BBLInstructionCost(CostSite site, BBLID bbl) {
        if (_inOpenMPRegion[bbl]) {
            // infomsg() << "wow" << bbl << std::endl;
            return _BBL_instruction_cost[site][bbl] * _instruction_multiplier[site] / _ilp[site] / _core_count[site];
        }
        else {
            return _BBL_instruction_cost[site][bbl] * _instruction_multiplier[site] / _ilp[site];
        }
        
    }
    inline COST BBLMemoryCost(CostSite site, BBLID bbl) {
        if (_inOpenMPRegion[bbl]) {
            return _BBL_memory_cost[site][bbl] / _mlp[site] / _core_count[site];
        }
        else {
            return _BBL_memory_cost[site][bbl] / _mlp[site];
        }
    }
};

} // namespace PIMProf

#endif // __COSTPACKAGE_H__