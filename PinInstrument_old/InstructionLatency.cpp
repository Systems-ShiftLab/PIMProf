//===- InstructionLatency.cpp - Utils for instrumentation ------------*- C++ -*-===//
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
#include "InstructionLatency.h"
#include "Cache.h"

using namespace PIMProf;

/* ===================================================================== */
/* Static data structure */
/* ===================================================================== */

COST InstructionLatency::_instruction_latency[MAX_COST_SITE][MAX_INDEX];


/* ===================================================================== */
/* Global data structure */
/* ===================================================================== */
long long int instr_cnt = 0, mem_instr_cnt = 0, nonmem_instr_cnt = 0;
extern BBLScope bbl_scope;

/* ===================================================================== */
/* InstructionLatency */
/* ===================================================================== */


InstructionLatency::InstructionLatency()
{
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < MAX_INDEX; j++) {
            _instruction_latency[i][j] = 1;
        }
    }
}

InstructionLatency::InstructionLatency(const std::string filename)
{
    InstructionLatency();
    ReadConfig(filename);
}

VOID InstructionLatency::SetBBLSize(BBLID _BBL_size) {
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        CostSolver::_BBL_instruction_cost[i].resize(_BBL_size);
        memset(&CostSolver::_BBL_instruction_cost[i][0], 0, _BBL_size * sizeof CostSolver::_BBL_instruction_cost[i][0]);
    }
}

VOID InstructionLatency::InstructionCount(UINT32 opcode, BOOL ismem)
{
    instr_cnt++;
    BBLID bblid = bbl_scope.top();
    if (bblid == GLOBALBBLID) return;


    if (ismem) {
        mem_instr_cnt++;
    }
    else {
        nonmem_instr_cnt++;
        for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
            CostSolver::_BBL_instruction_cost[i][bblid] += _instruction_latency[i][opcode];
        }
    }
}

VOID InstructionLatency::InstructionInstrument(INS ins, VOID *v)
{
    UINT32 opcode = (UINT32)(INS_Opcode(ins));
    BOOL ismem = INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)InstructionCount,
        IARG_ADDRINT, opcode,
        IARG_BOOL, ismem,
        IARG_END);
}


VOID InstructionLatency::ReadConfig(const std::string filename)
{
    INIReader reader(filename);
    ASSERTX(!INIErrorMsg(reader.ParseError(), filename, std::cerr));
    
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < MAX_INDEX; j++) {
            std::string opcodestr = OPCODE_StringShort(j);
            if (opcodestr != "LAST") {
                COST latency = reader.GetReal(CostSiteName[i] + "InstructionLatency", opcodestr, -1);
                if (latency >= 0) {
                    _instruction_latency[i][j] = latency;
                }
            }
        }
    }
}

std::ostream& InstructionLatency::WriteConfig(std::ostream& out)
{
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        out << ("[" + CostSiteName[i] + "InstructionLatency]") << std::endl
            << "; <Instuction Name> = <Instruction Latency>" << std::endl;
        for (UINT32 j = 0; j < MAX_INDEX; j++)
        {
            std::string opcodestr = OPCODE_StringShort(j);
            if (opcodestr != "LAST") {
                opcodestr = ljstr(opcodestr, 15);
                out << opcodestr << "= " << _instruction_latency[i][j] << std::endl;
            }
        }
        out << std::endl;
    }
    return out;
}

VOID InstructionLatency::WriteConfig(const std::string filename)
{
    std::ofstream out;
    out.open(filename.c_str(), ios_base::out);
    WriteConfig(out);
    out.close();
}