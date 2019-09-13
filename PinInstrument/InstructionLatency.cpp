//===- InstructionLatency.cpp - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "InstructionLatency.h"

using namespace PIMProf;

/* ===================================================================== */
/* InstructionLatency */
/* ===================================================================== */


void InstructionLatency::initialize(BBLScope *scope, BBLID bbl_size)
{
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < MAX_INDEX; j++) {
            _instruction_latency[i][j] = 1;
        }
    }
    _instr_cnt = 0;
    _mem_instr_cnt = 0;
    _nonmem_instr_cnt = 0;
    _bbl_scope = scope;
    _bbl_size = bbl_size;
    SetBBLSize(_bbl_size);
}

void InstructionLatency::initialize(BBLScope *scope, BBLID bbl_size, ConfigReader &reader)
{
    infomsg() << "start initialize" << std::endl;
    initialize(scope, bbl_size);
    ReadConfig(reader);
}

VOID InstructionLatency::SetBBLSize(BBLID bbl_size) {
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        _BBL_instruction_cost[i].resize(bbl_size);
        memset(&_BBL_instruction_cost[i][0], 0, bbl_size * sizeof _BBL_instruction_cost[i][0]);
    }
}


void InstructionLatency::instrument() {
    INS_AddInstrumentFunction(InstructionInstrument, (VOID *)this);
}


VOID InstructionLatency::InstructionCount(InstructionLatency *self, UINT32 opcode, BOOL ismem)
{
    self->_instr_cnt++;
    BBLID bblid = self->_bbl_scope->top();
    if (bblid == GLOBALBBLID) return;

    if (ismem) {
        self->_mem_instr_cnt++;
    }
    else {
        self->_nonmem_instr_cnt++;
        for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
            self->_BBL_instruction_cost[i][bblid] += self->_instruction_latency[i][opcode];
        }
    }
}

VOID InstructionLatency::InstructionInstrument(INS ins, VOID *void_self)
{
    InstructionLatency *self = (InstructionLatency *)void_self;
    UINT32 opcode = (UINT32)(INS_Opcode(ins));
    BOOL ismem = INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)InstructionCount,
        IARG_PTR, (VOID *)self,
        IARG_ADDRINT, opcode,
        IARG_BOOL, ismem,
        IARG_END);
}


VOID InstructionLatency::ReadConfig(ConfigReader &reader)
{
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