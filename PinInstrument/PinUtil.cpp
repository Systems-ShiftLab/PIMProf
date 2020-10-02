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

void CommandLineParser::initialize(int argc, char *argv[])
{
    // if(PIN_Init(argc, argv))
    // {
    //     Usage(errormsg());
    // }

    // _configfile = KnobConfig.Value();
    // _outputfile = KnobOutput.Value();
    // _statsfile = KnobStats.Value();
    // _enableroi = KnobEnableROI;
    // _enableroidecision = KnobEnableROIDecision;

    // if (_configfile == "") {
    //     errormsg() << "No config file provided." << std::endl;
    //     assert(0);
    // }
    // if (_outputfile == "") {
    //     _outputfile = "decision.out";
    //     warningmsg() << "No output file name specified. Printing output to " + _outputfile + "." << std::endl;
    // }
    // if (_statsfile == "") {
    //     _statsfile = "CacheStats.out";
    //     warningmsg() << "No statistic file name specified. Printing output to " + _statsfile + "." << std::endl;
    // }
}

void PIMProf::PrintInstruction(std::ostream *out, uint64_t insAddr, std::string insDis, uint32_t simd_len) {
    *out << std::hex << insAddr << std::dec << ", " << insDis << " " << simd_len << std::endl;
    // *out << insDis << std::endl;
}

void PIMProf::PrintInfo(std::ostream *out, std::string info) {
    *out << info << std::endl;
}