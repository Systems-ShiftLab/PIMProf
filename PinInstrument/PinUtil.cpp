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
// KNOB<std::string> KnobControlFlow(
//     KNOB_MODE_WRITEONCE,
//     "pintool",
//     "b", "",
//     "specify file name containing control flow graph information");
KNOB<std::string> KnobOutput(
    KNOB_MODE_WRITEONCE,
    "pintool",
    "o", "",
    "specify file name containing PIM offloading decision");

void CommandLineParser::initialize(int argc, char *argv[])
{
    if(PIN_Init(argc, argv))
    {
        Usage(errormsg());
    }

    _configfile = KnobConfig.Value();
    _outputfile = KnobOutput.Value();
    // _controlflowfile = KnobControlFlow.Value();

    if (_configfile == "") {
        errormsg() << "No config file provided." << std::endl;
        ASSERTX(0);
    }
    if (_outputfile == "") {
        _outputfile = "decision.out";
        warningmsg() << "No output file name specified. Printing output to file offload_decision.txt." << std::endl;
    }
    // if (_controlflowfile == "") {
    //     errormsg() << "Control flow graph file correpsonding to the input program not provided." << std::endl;
    //     ASSERTX(0);
    // }
}