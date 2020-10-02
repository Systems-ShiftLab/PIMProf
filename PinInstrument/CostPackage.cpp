//===- CostPackage.cpp - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#include "CostPackage.h"

using namespace PIMProf;

/* ===================================================================== */
/* CostPackage */
/* ===================================================================== */

void CostPackage::initialize(int argc, char *argv[])
{
    _command_line_parser.initialize(argc, argv);
    _config_reader = ConfigReader(_command_line_parser.configfile());
    // initialize global BBL
    // initializeNewBBL(UUID(0, 0));
}

// void CostPackage::initializeNewBBL(UUID bblhash) {
//     _bbl_hash[bblhash] = _bbl_size;
//     _bbl_size++;
//     if (_command_line_parser.enableroi()) {
//         _bbl_parallelizable.push_back(true);
//     }
//     else {
//         _bbl_parallelizable.push_back(false);
//     }
//     for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
//         _bbl_instruction_cost[i].push_back(0);
//     }
//     for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
//         _bbl_memory_cost[i].push_back(0);
//     }
//     if (_command_line_parser.enableroidecision()) {
//         _roi_decision.push_back(CostSite::CPU);
//     }
//     for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
//         _bbl_instruction_memory_cost[i].push_back(0);
//     }
// #ifdef PIMPROF_MPKI
//     _bbl_visit_cnt.push_back(0);
//     _bbl_instr_cnt.push_back(0);
//     _simd_instr_cnt.push_back(0);
//     _cache_miss.push_back(0);
//     for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
//         for (uint32_t j = 0; j < MAX_LEVEL; j++) {
//             _bbl_storage_level_cost[i][j].push_back(0);
//         }
//     }
// #endif
// }