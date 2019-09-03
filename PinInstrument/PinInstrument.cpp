//===- PinInstrument.cpp - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <unistd.h>
#include <cmath>


#include "../LLVMAnalysis/Common.h"
#include "INIReader.h"
#include "PinInstrument.h"

using namespace PIMProf;

/* ===================================================================== */
/* Static data structure */
/* ===================================================================== */

MemoryLatency PinInstrument::memory_latency;
InstructionLatency PinInstrument::instruction_latency;
DataReuse PinInstrument::data_reuse;
CostSolver PinInstrument::solver;
std::stack<BBLID> BBLScope::bblidstack;

bool inOpenMPRegion;

/* ===================================================================== */
/* Global data structure */
/* ===================================================================== */
BBLScope bbl_scope;


/* ===================================================================== */
/* PinInstrument */
/* ===================================================================== */

VOID PinInstrument::DoAtAnnotatorHead(BBLID bblid, INT32 isomp)
{
    // std::cout << std::dec << "PIMProfHead: " << bblid << std::endl;
    bbl_scope.push(bblid);
}

VOID PinInstrument::DoAtAnnotatorTail(BBLID bblid, INT32 isomp)
{
    // std::cout << std::dec << "PIMProfTail: " << bblid << std::endl;
    ASSERTX(bbl_scope.GetCurrentBBL() == bblid);
    bbl_scope.pop();
    inOpenMPRegion = false;
}

VOID PinInstrument::ImageInstrument(IMG img, VOID *v)
{
    // push a fake bblid

    bbl_scope.push(GLOBALBBLID);
    inOpenMPRegion = false;

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
            IARG_FUNCARG_CALLSITE_VALUE, 0, // The first argument of DoAtAnnotatorHead
            IARG_FUNCARG_CALLSITE_VALUE, 1, // The second argument of DoAtAnnotatorTail
            IARG_END);
        RTN_Close(annotator_head);

        RTN_Open(annotator_tail);
        RTN_InsertCall(
            annotator_tail,
            IPOINT_BEFORE,
            (AFUNPTR)DoAtAnnotatorTail,
            IARG_FUNCARG_CALLSITE_VALUE, 0, // The first argument of DoAtAnnotatorTail
            IARG_FUNCARG_CALLSITE_VALUE, 1, // The second argument of DoAtAnnotatorTail
            IARG_END);
        RTN_Close(annotator_tail);
    }
}

VOID PinInstrument::FinishInstrument(INT32 code, VOID *v)
{
    char *outputfile = (char *)v;
    std::ofstream ofs(outputfile, std::ofstream::out);
    delete outputfile;
    CostSolver::DECISION decision = CostSolver::PrintSolution(ofs);
    ofs.close();
    
    ofs.open("BBLBlockCost.out", std::ofstream::out);
    ofs << "BBL\t" << "Decision\t"
    << "CPUIns\t\t" << "PIMIns\t\t"
    << "CPUMem\t\t" << "PIMMem\t\t"
    << "difference" << std::endl;
    for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
        ofs << i << "\t"
        << (decision[i] == CPU ? "C" : "") << (decision[i] == PIM ? "P" : "") << "\t"
        << CostSolver::_BBL_instruction_cost[CPU][i] *
           CostSolver::_instruction_multiplier[CPU]
        << "\t\t"
        << CostSolver::_BBL_instruction_cost[PIM][i] * 
           CostSolver::_instruction_multiplier[PIM]
        << "\t\t"
        << CostSolver::_BBL_memory_cost[CPU][i] << "\t\t"
        << CostSolver::_BBL_memory_cost[PIM][i] << "\t\t"
        << CostSolver::_BBL_partial_total[CPU][i] - CostSolver::_BBL_partial_total[PIM][i] << std::endl;
    }
    ofs.close();
    ofs.open("BBLReuseCost.dot", std::ofstream::out);
    DataReuse::print(ofs, DataReuse::getRoot());
    ofs.close();
}
