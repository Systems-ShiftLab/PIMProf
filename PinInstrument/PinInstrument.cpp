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
#include "Cache.h"

#include "INIReader.h"

using namespace PIMProf;



/* ===================================================================== */
/* InstructionLatency */
/* ===================================================================== */

UINT32 InstructionLatency::latencytable[MAX_INDEX];

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

// BOOL InstructionLatency::IsMemReadIndex(UINT32 i)
// {
//     return (INDEX_MEM_READ_SIZE <= i && i < INDEX_MEM_READ_SIZE + MAX_MEM_SIZE );
// }

// BOOL InstructionLatency::IsMemWriteIndex(UINT32 i)
// {
//     return (INDEX_MEM_WRITE_SIZE <= i && i < INDEX_MEM_WRITE_SIZE + MAX_MEM_SIZE );
// }

// UINT32 InstructionLatency::INS_GetIndex(INS ins)
// {
//     if( INS_IsPredicated(ins) )
//         return MAX_INDEX + INS_Opcode(ins);
//     else
//         return INS_Opcode(ins);
// }


// UINT32 InstructionLatency::IndexStringLength(BBL bbl, BOOL memory_acess_profile)
// {
//     UINT32 count = 0;

//     for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
//     {
//         count++;
//         if( memory_acess_profile )
//         {
//             if( INS_IsMemoryRead(ins) ) count++;   // for size

//             if( INS_IsStackRead(ins) ) count++;

//             if( INS_IsIpRelRead(ins) ) count++;


//             if( INS_IsMemoryWrite(ins) ) count++; // for size

//             if( INS_IsStackWrite(ins) ) count++;

//             if( INS_IsIpRelWrite(ins) ) count++;


//             if( INS_IsAtomicUpdate(ins) ) count++;
//         }
//     }

//     return count;
// }

// UINT32 InstructionLatency::MemsizeToIndex(UINT32 size, BOOL write)
// {
//     return (write ? INDEX_MEM_WRITE_SIZE : INDEX_MEM_READ_SIZE ) + size;
// }

// UINT16 *InstructionLatency::INS_GenerateIndexString(INS ins, UINT16 *stats, BOOL memory_acess_profile)
// {
//     *stats++ = INS_GetIndex(ins);

//     if( memory_acess_profile )
//     {
//         if( INS_IsMemoryRead(ins) )  *stats++ = MemsizeToIndex( INS_MemoryReadSize(ins), 0 );
//         if( INS_IsMemoryWrite(ins) ) *stats++ = MemsizeToIndex( INS_MemoryWriteSize(ins), 1 );

//         if( INS_IsAtomicUpdate(ins) ) *stats++ = INDEX_MEM_ATOMIC;

//         if( INS_IsStackRead(ins) ) *stats++ = INDEX_STACK_READ;
//         if( INS_IsStackWrite(ins) ) *stats++ = INDEX_STACK_WRITE;

//         if( INS_IsIpRelRead(ins) ) *stats++ = INDEX_IPREL_READ;
//         if( INS_IsIpRelWrite(ins) ) *stats++ = INDEX_IPREL_WRITE;
//     }

//     return stats;
// }

// string InstructionLatency::IndexToOpcodeString( UINT32 index )
// {
//     if( INDEX_SPECIAL <= index  && index < INDEX_SPECIAL_END)
//     {
//         if( index == INDEX_TOTAL )            return  "*total";
//         else if( IsMemReadIndex(index) )      return  "*mem-read-" + decstr( index - INDEX_MEM_READ_SIZE );
//         else if( IsMemWriteIndex(index))      return  "*mem-write-" + decstr( index - INDEX_MEM_WRITE_SIZE );
//         else if( index == INDEX_MEM_ATOMIC )  return  "*mem-atomic";
//         else if( index == INDEX_STACK_READ )  return  "*stack-read";
//         else if( index == INDEX_STACK_WRITE ) return  "*stack-write";
//         else if( index == INDEX_IPREL_READ )  return  "*iprel-read";
//         else if( index == INDEX_IPREL_WRITE ) return  "*iprel-write";

//         else
//         {
//             ASSERTX(0);
//             return "";
//         }
//     }
//     else
//     {
//         return OPCODE_StringShort(index);
//     }

// }


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

std::ostream& InstructionLatency::WriteConfig(std::ostream& out)
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
    return out;
}

VOID InstructionLatency::WriteConfig(const std::string filename)
{
    std::ofstream out;
    out.open(filename.c_str(), ios_base::out);
    WriteConfig(out);
    out.close();
}

CACHE MemoryLatency::cache;

VOID MemoryLatency::InsRef(ADDRINT addr)
{
    cache.InsRef(addr);
}

VOID MemoryLatency::MemRefMulti(ADDRINT addr, UINT32 size, CACHE_LEVEL_BASE::ACCESS_TYPE accessType)
{
    cache.MemRefMulti(addr, size, accessType);
}

VOID MemoryLatency::MemRefSingle(ADDRINT addr, UINT32 size, CACHE_LEVEL_BASE::ACCESS_TYPE accessType)
{
    cache.MemRefSingle(addr, size, accessType);
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
        const AFUNPTR countFun = (size <= 4 ? (AFUNPTR)MemRefSingle : (AFUNPTR)MemRefMulti);

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
        const AFUNPTR countFun = (size <= 4 ? (AFUNPTR)MemRefSingle : (AFUNPTR)MemRefMulti);

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
    cache.WriteStats("stats.out");
}

VOID MemoryLatency::ReadConfig(const std::string filename)
{
    cache.ReadConfig(filename);
}

std::ostream& MemoryLatency::WriteConfig(std::ostream& out)
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
    return out;
}

VOID MemoryLatency::WriteConfig(const std::string filename)
{
    ofstream out;
    out.open(filename.c_str(), ios_base::out);
    WriteConfig(out);
    out.close();
}
