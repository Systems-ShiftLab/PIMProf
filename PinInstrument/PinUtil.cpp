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

/* ===================================================================== */
/* Local command line switches */
/* ===================================================================== */

KNOB<std::string> KnobConfig(
    KNOB_MODE_WRITEONCE,
    "pintool",
    "c", "",
    "specify config file name");
KNOB<std::string> KnobOutput(
    KNOB_MODE_WRITEONCE,
    "pintool",
    "o", "",
    "specify file name containing PIM offloading decision");
KNOB<std::string> KnobStats(
    KNOB_MODE_WRITEONCE,
    "pintool",
    "s", "",
    "specify file name for statistics");
KNOB<bool> KnobEnableROI(
    KNOB_MODE_WRITEONCE,
    "pintool",
    "roi", "0",
    "specify whether ROI mode is enabled, if enabled, only regions between PIMProfROIHead and PIMProfROITail is analyzed");
KNOB<bool> KnobEnableROIDecision(
    KNOB_MODE_WRITEONCE,
    "pintool",
    "roidecision", "0",
    "specify whether ROI decision mode is enabled, if enabled, regions between PIMProfROIHead and PIMProfROITail will be offloaded to PIM and the rest stay on CPU");

void CommandLineParser::initialize(int argc, char *argv[])
{
    if(PIN_Init(argc, argv))
    {
        Usage(errormsg());
    }

    _configfile = KnobConfig.Value();
    _outputfile = KnobOutput.Value();
    _statsfile = KnobStats.Value();
    _enableroi = KnobEnableROI;
    _enableroidecision = KnobEnableROIDecision;

    if (_configfile == "") {
        errormsg() << "No config file provided." << std::endl;
        ASSERTX(0);
    }
    if (_outputfile == "") {
        _outputfile = "decision.out";
        warningmsg() << "No output file name specified. Printing output to " + _outputfile + "." << std::endl;
    }
    if (_statsfile == "") {
        _statsfile = "CacheStats.out";
        warningmsg() << "No statistic file name specified. Printing output to " + _statsfile + "." << std::endl;
    }
}

VOID PIMProf::PrintInstruction(std::ostream *out, UINT64 insAddr, std::string insDis) {
    *out << std::hex << insAddr << std::dec << " " << insDis << std::endl;
}

VOID PIMProf::PrintInfo(std::ostream *out, std::string info) {
    *out << info << std::endl;
}