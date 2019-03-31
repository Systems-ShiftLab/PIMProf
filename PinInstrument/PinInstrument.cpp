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


#include "../LLVMAnalysis/Common.h"
#include "PinInstrument.h"

#include "INIReader.h"

using namespace PIMProf;

/* ===================================================================== */
/* CACHE DEFINITION */
/* ===================================================================== */

LOCALFUN ITLB::CACHE itlb("ITLB", ITLB::cacheSize, ITLB::lineSize, ITLB::associativity);
LOCALVAR DTLB::CACHE dtlb("DTLB", DTLB::cacheSize, DTLB::lineSize, DTLB::associativity);
LOCALVAR IL1::CACHE il1("L1 Instruction Cache", IL1::cacheSize, IL1::lineSize, IL1::associativity);
LOCALVAR DL1::CACHE dl1("L1 Data Cache", DL1::cacheSize, DL1::lineSize, DL1::associativity);
LOCALVAR UL2::CACHE ul2("L2 Unified Cache", UL2::cacheSize, UL2::lineSize, UL2::associativity);
LOCALVAR UL3::CACHE ul3("L3 Unified Cache", UL3::cacheSize, UL3::lineSize, UL3::associativity);


/* ===================================================================== */
/* InstructionLatency */
/* ===================================================================== */


InstructionLatency::InstructionLatency()
{
    for (UINT32 i = 0; i < MAX_INDEX; i++) {
        latencytable[i] = 1;
    }
    ReadConfig("defaultlatency.ini");
}

InstructionLatency::InstructionLatency(const std::string filename)
{
    InstructionLatency();
    ReadConfig(filename);
}

VOID InstructionLatency::ReadConfig(const std::string filename)
{
    INIReader reader(filename);
    for (UINT32 i = 0; i < MAX_INDEX; i++) {
        std::string opcodestr = OPCODE_StringShort(i);
        if (opcodestr != "LAST") {
            long latency = reader.GetInteger("InstructionLatency", opcodestr, -1);
            if (latency >= 0) {
                latencytable[i] = latency;
            }
        }
    }
}

VOID InstructionLatency::WriteConfig(ostream& out)
{
    out << "[InstructionLatency]" << std::endl
        << "; <Instuction Name> = <Instruction Latency>" << std::endl;
    for (UINT32 i = 0; i < MAX_INDEX; i++)
    {
        std::string opcodestr = OPCODE_StringShort(i);
        if (opcodestr != "LAST") {
            opcodestr = ljstr(opcodestr, 15);
            out << opcodestr << "= " << latencytable[i] << std::endl;
        }
    }
}

VOID InstructionLatency::WriteConfig(const std::string filename)
{
    ofstream out;
    out.open(filename.c_str(), ios_base::out);
    WriteConfig(out);
    out.close();
}

VOID MemoryLatency::Instruction(INS ins, VOID *v)
{
    // all instruction fetches access I-cache
    INS_InsertCall(
        ins, IPOINT_BEFORE, (AFUNPTR)InsRef,
        IARG_INST_PTR,
        IARG_END);

    if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins))
    {
        const UINT32 size = INS_MemoryReadSize(ins);
        const AFUNPTR countFun = (size <= 4 ? (AFUNPTR) MemRefSingle : (AFUNPTR) MemRefMulti);

        // only predicated-on memory instructions access D-cache
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, countFun,
            IARG_MEMORYREAD_EA,
            IARG_MEMORYREAD_SIZE,
            IARG_UINT32, CACHE_LEVEL_BASE::ACCESS_TYPE_LOAD,
            IARG_END);
    }

    if (INS_IsMemoryWrite(ins) && INS_IsStandardMemop(ins))
    {
        const UINT32 size = INS_MemoryWriteSize(ins);
        const AFUNPTR countFun = (size <= 4 ? (AFUNPTR) MemRefSingle : (AFUNPTR) MemRefMulti);

        // only predicated-on memory instructions access D-cache
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, countFun,
            IARG_MEMORYWRITE_EA,
            IARG_MEMORYWRITE_SIZE,
            IARG_UINT32, CACHE_LEVEL_BASE::ACCESS_TYPE_STORE,
            IARG_END);
    }
}


VOID MemoryLatency::Fini(INT32 code, VOID * v)
{
    std::cerr << itlb;
    std::cerr << dtlb;
    std::cerr << il1;
    std::cerr << dl1;
    std::cerr << ul2;
    std::cerr << ul3;
}

VOID MemoryLatency::ReadConfig(const std::string filename)
{
    // INIReader reader(filename);
    // for (UINT32 i = 0; i < MAX_INDEX; i++) {
    //     std::string opcodestr = OPCODE_StringShort(i);
    //     if (opcodestr != "LAST") {
    //         long latency = reader.GetInteger("InstructionLatency", opcodestr, -1);
    //         if (latency >= 0) {
    //             instruction_latency.latencytable[i] = latency;
    //         }
    //     }
    // }
}

VOID MemoryLatency::WriteConfig(ostream& out)
{
    // out << "[InstructionLatency]" << std::endl;
    // for (UINT32 i = 0; i < MAX_INDEX; i++)
    // {
    //     std::string opcodestr = OPCODE_StringShort(i);
    //     if (opcodestr != "LAST") {
    //         opcodestr = ljstr(opcodestr, 15);
    //         out << opcodestr << "= " << instruction_latency.latencytable[i] << std::endl;
    //     }
    // }
}

VOID MemoryLatency::WriteConfig(const std::string filename)
{
    ofstream out;
    out.open(filename.c_str(), ios_base::out);
    WriteConfig(out);
    out.close();
}
