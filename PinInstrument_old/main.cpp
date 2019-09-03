
#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <unistd.h>
#include "control_manager.H"

#include "../LLVMAnalysis/Common.h"
#include "pin.H"
#include "PinInstrument.h"

using namespace CONTROLLER;
using namespace PIMProf;

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobConfig(
    KNOB_MODE_WRITEONCE,
    "pintool",
    "c", "",
    "specify config file name");
KNOB<string> KnobControlFlow(
    KNOB_MODE_WRITEONCE,
    "pintool",
    "b", "",
    "specify file name containing control flow graph information");
KNOB<string> KnobOutput(
    KNOB_MODE_WRITEONCE,
    "pintool",
    "o", "",
    "specify file name containing PIM offloading decision");

INT32 Usage(std::ostream &out) {
    out << "This pin tool estimates the performance of the given program on a CPU-PIM configuration."
        << std::endl
        << KNOB_BASE::StringKnobSummary()
        << std::endl;
    return -1;
}

int main(int argc, CHAR *argv[])
{
    PIN_InitSymbols();

    if( PIN_Init(argc,argv) )
    {
        return Usage(std::cerr);
    }

    if (std::getenv("PIMPROF_ROOT") == NULL) {
        errormsg("Environment variable PIMPROF_ROOT not set.");
        ASSERTX(0);
    }
    string rootdir = std::getenv("PIMPROF_ROOT");
    string configfile = KnobConfig.Value();
    string outputfile = KnobOutput.Value();

    if (configfile == "") {
        configfile = rootdir + "/PinInstrument/defaultconfig.ini";
        warningmsg("No config file provided. Using default config file.");
    }
    if (outputfile == "") {
        outputfile = "offload_decision.txt";
        warningmsg("No output file name specified. Printing output to file offload_decision.txt.");
    }

    InstructionLatency::ReadConfig(configfile);
    MemoryLatency::ReadConfig(configfile);
    CostSolver::ReadConfig(configfile);

    string controlflowfile = KnobControlFlow.Value();
    if (controlflowfile == "") {
        std::cerr << REDCOLOR << "## PIMProf ERROR: " << NOCOLOR << "Control flow graph file correpsonding to the input program not provided.\n\n";
        ASSERTX(0);
    }
    CostSolver::ReadControlFlowGraph(controlflowfile);

    IMG_AddInstrumentFunction(PinInstrument::ImageInstrument, 0);

    INS_AddInstrumentFunction(MemoryLatency::InstructionInstrument, 0);
    INS_AddInstrumentFunction(InstructionLatency::InstructionInstrument, 0);

    PIN_AddFiniFunction(MemoryLatency::FinishInstrument, 0);

    char *outputfile_char = new char[outputfile.length() + 1];
    strcpy(outputfile_char, outputfile.c_str());
    PIN_AddFiniFunction(PinInstrument::FinishInstrument, (VOID *)(outputfile_char));

    // Never returns
    PIN_StartProgram();
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
