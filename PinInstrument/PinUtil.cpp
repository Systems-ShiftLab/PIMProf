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
KNOB<bool> KnobEnableROI(
    KNOB_MODE_WRITEONCE,
    "pintool",
    "roi", "0",
    "specify whether ROI mode is enabled, if enabled, only regions between PIMProfROIHead and PIMProfROITail is analyzed");

void CommandLineParser::initialize(int argc, char *argv[])
{
    if(PIN_Init(argc, argv))
    {
        Usage(errormsg());
    }

    _configfile = KnobConfig.Value();
    _outputfile = KnobOutput.Value();
    _enableroi = KnobEnableROI;

    if (_configfile == "") {
        errormsg() << "No config file provided." << std::endl;
        ASSERTX(0);
    }
    if (_outputfile == "") {
        _outputfile = "decision.out";
        warningmsg() << "No output file name specified. Printing output to " + _outputfile + "." << std::endl;
    }
}