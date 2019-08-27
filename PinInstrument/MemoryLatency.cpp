//===- MemoryLatency.cpp - Utils for instrumentation ------------*- C++ -*-===//
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
#include "MemoryLatency.h"
#include "CostSolver.h"
#include "Cache.h"

using namespace PIMProf;

/* ===================================================================== */
/* Static data structure */
/* ===================================================================== */

CACHE MemoryLatency::cache;

/* ===================================================================== */
/* MemoryLatency */
/* ===================================================================== */

VOID MemoryLatency::InstrCacheRef(ADDRINT addr)
{
    cache.InstrCacheRef(addr);
}

VOID MemoryLatency::DataCacheRefMulti(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    cache.DataCacheRefMulti(addr, size, accessType);
}

VOID MemoryLatency::DataCacheRefSingle(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    cache.DataCacheRefSingle(addr, size, accessType);
}

VOID MemoryLatency::InstructionInstrument(INS ins, VOID *v)
{
    // all instruction fetches access I-cache
    INS_InsertCall(
        ins, IPOINT_BEFORE, (AFUNPTR)InstrCacheRef,
        IARG_INST_PTR,
        IARG_END);
    if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins))
    {
        const UINT32 size = INS_MemoryReadSize(ins);
        const AFUNPTR countFun = (size <= 4 ? (AFUNPTR)DataCacheRefSingle : (AFUNPTR)DataCacheRefMulti);

        // only predicated-on memory instructions access D-cache
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, countFun,
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
            IARG_MEMORYWRITE_EA,
            IARG_MEMORYWRITE_SIZE,
            IARG_UINT32, ACCESS_TYPE_STORE,
            IARG_END);
    }
}

VOID MemoryLatency::FinishInstrument(INT32 code, VOID *v)
{
    cache.WriteStats("stats.out");
}

VOID MemoryLatency::ReadConfig(const std::string filename)
{
    cache.ReadConfig(filename);
    
    INIReader reader(filename);
    ASSERTX(!INIErrorMsg(reader.ParseError(), filename, std::cerr));
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        COST cost = reader.GetReal("Memory", CostSiteName[i] + "memorycost", -1);
        if (cost >= 0) {
            CostSolver::_memory_cost[i] = cost;
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