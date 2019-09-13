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
/* Local command line switches */
/* ===================================================================== */

KNOB<std::string> KnobConfig(
    KNOB_MODE_WRITEONCE,
    "pintool",
    "c", "",
    "specify config file name");
KNOB<std::string> KnobControlFlow(
    KNOB_MODE_WRITEONCE,
    "pintool",
    "b", "",
    "specify file name containing control flow graph information");
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

    if (std::getenv("PIMPROF_ROOT") == NULL) {
        errormsg() << "Environment variable PIMPROF_ROOT not set." << std::endl;
        ASSERTX(0);
    }
    _rootdir = std::getenv("PIMPROF_ROOT");
    _configfile = KnobConfig.Value();
    _outputfile = KnobOutput.Value();
    _controlflowfile = KnobControlFlow.Value();

    if (_configfile == "") {
        _configfile = _rootdir + "/PinInstrument/defaultconfig.ini";
        warningmsg() << "No config file provided. Using default config file." << std::endl;
    }
    if (_outputfile == "") {
        _outputfile = "offload_decision.txt";
        warningmsg() << "No output file name specified. Printing output to file offload_decision.txt." << std::endl;
    }
    if (_controlflowfile == "") {
        errormsg() << "Control flow graph file correpsonding to the input program not provided." << std::endl;
        ASSERTX(0);
    }
}