//===- PinUtil.cpp - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "PinUtil.h"

using namespace PIMProf;
/* ===================================================================== */
/* Typedefs and constants */
/* ===================================================================== */
const std::string PIMProf::CostSiteName[MAX_COST_SITE] = { "CPU", "PIM" };

const std::string PIMProf::StorageLevelName[MAX_LEVEL] = {
    "IL1", "DL1", "UL2", "UL3", "MEM"
};

void Usage()
{
    errormsg("Usage: ./Solver.exe <cpu_stats_file> <pim_stats_file> <output_file> <mode>");
    errormsg("Select mode from: mpki, para, dep");
}

void CommandLineParser::initialize(int argc, char *argv[])
{
    if (argc != 5) {
        Usage();
        assert(0);
    }
    _cpustatsfile = argv[1];
    _pimstatsfile = argv[2];
    _outputfile = argv[3];
    _mode = argv[4];
    if (_mode != "mpki" && _mode != "para" && _mode != "dep") {
        Usage();
        assert(0);
    }
}

void PIMProf::PrintInstruction(std::ostream *out, uint64_t insAddr, std::string insDis, uint32_t simd_len) {
    *out << std::hex << insAddr << std::dec << ", " << insDis << " " << simd_len << std::endl;
    // *out << insDis << std::endl;
}

void PIMProf::PrintInfo(std::ostream *out, std::string info) {
    *out << info << std::endl;
}