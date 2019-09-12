//===- PinInstrument.cpp - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#include "PinInstrument.h"

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
    out << "Invalid argument."
        << std::endl
        << KNOB_BASE::StringKnobSummary()
        << std::endl;
    return -1;
}

/* ===================================================================== */
/* Static data structure */
/* ===================================================================== */

// MemoryLatency PinInstrument::memory_latency;
// InstructionLatency PinInstrument::instruction_latency;
// DataReuse PinInstrument::data_reuse;
// CostSolver PinInstrument::solver;

/* ===================================================================== */
/* PinInstrument */
/* ===================================================================== */


void PinInstrument::initialize(int argc, char *argv[])
{
    PIN_InitSymbols();

    if(PIN_Init(argc, argv))
    {
        Usage(errormsg());
    }

    if (std::getenv("PIMPROF_ROOT") == NULL) {
        errormsg() << "Environment variable PIMPROF_ROOT not set." << std::endl;
        ASSERTX(0);
    }
    string rootdir = std::getenv("PIMPROF_ROOT");
    string configfile = KnobConfig.Value();
    string outputfile = KnobOutput.Value();

    if (configfile == "") {
        configfile = rootdir + "/PinInstrument/defaultconfig.ini";
        warningmsg() << "No config file provided. Using default config file." << std::endl;
    }
    if (outputfile == "") {
        outputfile = "offload_decision.txt";
        warningmsg() << "No output file name specified. Printing output to file offload_decision.txt." << std::endl;
    }

    string controlflowfile = KnobControlFlow.Value();
    if (controlflowfile == "") {
        errormsg() << "Control flow graph file correpsonding to the input program not provided." << std::endl;
        ASSERTX(0);
    }
    ReadControlFlowGraph(controlflowfile);

    _inOpenMPRegion = false;
    _config_reader = ConfigReader(configfile);
    _instruction_latency.initialize(&_bbl_scope, _bbl_size, _config_reader);
}

void PinInstrument::ReadControlFlowGraph(const std::string filename)
{
    std::ifstream ifs;
    ifs.open(filename.c_str());
    std::string curline;

    getline(ifs, curline);
    std::stringstream ss(curline);
    ss >> _bbl_size;
    _bbl_size++; // bbl_size = Largest BBLID + 1
}


void PinInstrument::instrument()
{
    IMG_AddInstrumentFunction(ImageInstrument, (VOID *)this);
}

void PinInstrument::simulate()
{

    // string controlflowfile = KnobControlFlow.Value();
    // if (controlflowfile == "") {
    //     std::cerr << REDCOLOR << "## PIMProf ERROR: " << NOCOLOR << "Control flow graph file correpsonding to the input program not provided.\n\n";
    //     ASSERTX(0);
    // }
    // CostSolver::ReadControlFlowGraph(controlflowfile);

    instrument();
    _instruction_latency.instrument();
    // INS_AddInstrumentFunction(MemoryLatency::InstructionInstrument, 0);


    // PIN_AddFiniFunction(MemoryLatency::FinishInstrument, 0);

    // char *outputfile_char = new char[outputfile.length() + 1];
    // strcpy(outputfile_char, outputfile.c_str());
    // PIN_AddFiniFunction(PinInstrument::FinishInstrument, (VOID *)(outputfile_char));

    // Never returns
    PIN_StartProgram();
}

VOID PinInstrument::DoAtAnnotatorHead(PinInstrument *self, BBLID bblid, INT32 isomp)
{
    std::cout << std::dec << "PIMProfHead: " << bblid << std::endl;
    self->_bbl_scope.push(bblid);
}

VOID PinInstrument::DoAtAnnotatorTail(PinInstrument *self, BBLID bblid, INT32 isomp)
{
    std::cout << std::dec << "PIMProfTail: " << bblid << std::endl;
    ASSERTX(self->_bbl_scope.top() == bblid);
    self->_bbl_scope.pop();
    self->_inOpenMPRegion = false;
}

VOID PinInstrument::ImageInstrument(IMG img, VOID *void_self)
{
    // find annotator head and tail by their names
    RTN annotator_head = RTN_FindByName(img, PIMProfAnnotatorHead.c_str());
    RTN annotator_tail = RTN_FindByName(img, PIMProfAnnotatorTail.c_str());

    if (RTN_Valid(annotator_head) && RTN_Valid(annotator_tail))
    {
        // Instrument malloc() to print the input argument value and the return value.
        RTN_Open(annotator_head);
        RTN_InsertCall(
            annotator_head,
            IPOINT_BEFORE,
            (AFUNPTR)DoAtAnnotatorHead,
            IARG_PTR, void_self, // Pass the pointer of bbl_scope as an argument of DoAtAnnotatorHead
            IARG_FUNCARG_CALLSITE_VALUE, 0, // Pass the first function argument PIMProfAnnotatorHead as an argument of DoAtAnnotatorHead
            IARG_FUNCARG_CALLSITE_VALUE, 1, // Pass the second function argument PIMProfAnnotatorHead as an argument of DoAtAnnotatorHead
            IARG_END);
        RTN_Close(annotator_head);

        RTN_Open(annotator_tail);
        RTN_InsertCall(
            annotator_tail,
            IPOINT_BEFORE,
            (AFUNPTR)DoAtAnnotatorTail,
            IARG_PTR, void_self, // Pass the pointer of bbl_scope as an argument of DoAtAnnotatorHead
            IARG_FUNCARG_CALLSITE_VALUE, 0, // The first argument of DoAtAnnotatorTail
            IARG_FUNCARG_CALLSITE_VALUE, 1, // The second argument of DoAtAnnotatorTail
            IARG_END);
        RTN_Close(annotator_tail);
    }
}

VOID PinInstrument::FinishInstrument(INT32 code, VOID *void_self)
{
    // char *outputfile = (char *)v;
    // std::ofstream ofs(outputfile, std::ofstream::out);
    // delete outputfile;
    // CostSolver::DECISION decision = CostSolver::PrintSolution(ofs);
    // ofs.close();
    
    // ofs.open("BBLBlockCost.out", std::ofstream::out);

    // CostSolver::PrintBBLDecisionStat(ofs, decision, false);
    
    // ofs.close();
    // ofs.open("BBLReuseCost.dot", std::ofstream::out);
    // DataReuse::print(ofs, DataReuse::getRoot());
    // ofs.close();
}
