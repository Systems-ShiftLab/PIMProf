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

    _command_line_parser.initialize(argc, argv);
    
    ReadControlFlowGraph(_command_line_parser.controlflowfile());

    _inOpenMPRegion = false;
    _config_reader = ConfigReader(_command_line_parser.configfile());
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
