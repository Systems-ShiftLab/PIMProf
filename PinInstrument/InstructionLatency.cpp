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


void InstructionLatency::initialize(CostPackage *cost_package)
{
    _cost_package = cost_package;

    _cost_package->_instr_cnt = 0;
    _cost_package->_mem_instr_cnt = 0;
    _cost_package->_nonmem_instr_cnt = 0;

    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < MAX_INDEX; j++) {
            _cost_package->_instruction_latency[i][j] = 1;
        }
    }

    SetBBLSize(_cost_package->_bbl_size);
}

void InstructionLatency::initialize(CostPackage *cost_package, ConfigReader &reader)
{
    initialize(cost_package);
    ReadConfig(reader);
}

void InstructionLatency::SetBBLSize(BBLID bbl_size) {
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        _cost_package->_BBL_instruction_cost[i].resize(bbl_size);
        memset(&_cost_package->_BBL_instruction_cost[i][0], 0, bbl_size * sizeof _cost_package->_BBL_instruction_cost[i][0]);
    }
}


void InstructionLatency::instrument() {
    INS_AddInstrumentFunction(InstructionInstrument, (VOID *)this);
}


VOID InstructionLatency::InstructionCount(InstructionLatency *self, UINT32 opcode, BOOL ismem)
{
    self->_cost_package->_instr_cnt++;
    BBLID bblid = self->_cost_package->_bbl_scope.top();
    if (bblid == GLOBALBBLID) return;

    if (ismem) {
        self->_cost_package->_mem_instr_cnt++;
    }
    else {
        self->_cost_package->_nonmem_instr_cnt++;
        for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
            self->_cost_package->_BBL_instruction_cost[i][bblid] += self->_cost_package->_instruction_latency[i][opcode];
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


void InstructionLatency::ReadConfig(ConfigReader &reader)
{
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < MAX_INDEX; j++) {
            std::string opcodestr = OPCODE_StringShort(j);
            if (opcodestr != "LAST") {
                COST latency = reader.GetReal(CostSiteName[i] + "InstructionLatency", opcodestr, -1);
                if (latency >= 0) {
                    _cost_package->_instruction_latency[i][j] = latency;
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
                out << opcodestr << "= " << _cost_package->_instruction_latency[i][j] << std::endl;
            }
        }
        out << std::endl;
    }
    return out;
}

void InstructionLatency::WriteConfig(const std::string filename)
{
    std::ofstream out;
    out.open(filename.c_str(), ios_base::out);
    WriteConfig(out);
    out.close();
}

/* ===================================================================== */
/* MemoryLatency */
/* ===================================================================== */

void MemoryLatency::initialize(CACHE *cache, CostPackage *cost_package, ConfigReader &reader)
{
    _cache = cache;
    _cost_package = cost_package;
    ReadConfig(reader);
    SetBBLSize(_cost_package->_bbl_size);
}


void MemoryLatency::instrument()
{
    INS_AddInstrumentFunction(InstructionInstrument, (VOID *)this);
    PIN_AddFiniFunction(FinishInstrument, (VOID *)this);
}


VOID MemoryLatency::SetBBLSize(BBLID bbl_size) {
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        _cost_package->_BBL_memory_cost[i].resize(bbl_size);
        memset(&_cost_package->_BBL_memory_cost[i][0], 0, bbl_size * sizeof _cost_package->_BBL_memory_cost[i][0]);
    }
}

VOID MemoryLatency::ReadConfig(ConfigReader &reader)
{
    _cache->ReadConfig(reader);

    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        COST cost = reader.GetReal("Memory", CostSiteName[i] + "memorycost", -1);
        if (cost >= 0) {
            _cost_package->_memory_cost[i] = cost;
        }
    }
}

std::ostream& MemoryLatency::WriteConfig(std::ostream& out)
{
    // for (UINT32 i = 0; i < MAX_LEVEL; i++) {
    //     out << "[" << _name[i] << "]" << std::endl;
    //         << "linesize = " << _cache[i]->
    // }
    out << "Not implemented" << std::endl;
    return out;
}

VOID MemoryLatency::WriteConfig(const std::string filename)
{
    ofstream out;
    out.open(filename.c_str(), ios_base::out);
    WriteConfig(out);
    out.close();
}


VOID MemoryLatency::InstrCacheRef(MemoryLatency *self, ADDRINT addr)
{
    self->_cache->InstrCacheRef(addr);
}

VOID MemoryLatency::DataCacheRefMulti(MemoryLatency *self, ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    self->_cache->DataCacheRefMulti(addr, size, accessType);
}

VOID MemoryLatency::DataCacheRefSingle(MemoryLatency *self, ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    self->_cache->DataCacheRefSingle(addr, size, accessType);
}

VOID MemoryLatency::InstructionInstrument(INS ins, VOID *void_self)
{
    MemoryLatency *self = (MemoryLatency *)void_self;
    // all instruction fetches access I-cache
    INS_InsertCall(
        ins, IPOINT_BEFORE, (AFUNPTR)InstrCacheRef,
        IARG_PTR, (VOID *)self,
        IARG_INST_PTR,
        IARG_END);
    if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins))
    {
        const UINT32 size = INS_MemoryReadSize(ins);
        const AFUNPTR countFun = (size <= 4 ? (AFUNPTR)DataCacheRefSingle : (AFUNPTR)DataCacheRefMulti);

        // only predicated-on memory instructions access D-cache
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, countFun,
            IARG_PTR, (VOID *)self,
            IARG_MEMORYREAD_EA,
            IARG_MEMORYREAD_SIZE,
            IARG_UINT32, ACCESS_TYPE_LOAD,
            IARG_END);
    }
    if (INS_IsMemoryWrite(ins) && INS_IsStandardMemop(ins))
    {
        const UINT32 size = INS_MemoryWriteSize(ins);
        const AFUNPTR countFun = (size <= 4 ? (AFUNPTR)DataCacheRefSingle : (AFUNPTR)DataCacheRefMulti);

        // only predicated-on memory instructions access D-cache
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, countFun,
            IARG_PTR, (VOID *)self,
            IARG_MEMORYWRITE_EA,
            IARG_MEMORYWRITE_SIZE,
            IARG_UINT32, ACCESS_TYPE_STORE,
            IARG_END);
    }
}

VOID MemoryLatency::FinishInstrument(INT32 code, VOID *void_self)
{
    MemoryLatency *self = (MemoryLatency *)void_self;
    self->_cache->WriteStats("stats.out");
}