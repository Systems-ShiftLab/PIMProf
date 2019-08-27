//===- InstructionLatency.h - Utils for instrumentation --------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//



#ifndef __INSTRUCTIONLATENCY_H__
#define __INSTRUCTIONLATENCY_H__

#include <stack>
#include <list>
#include <set>
#include "pin.H"

#include "PinUtil.h"

#include "PinInstrument.h"
#include "MemoryLatency.h"
#include "CostSolver.h"
#include "Cache.h"

namespace PIMProf {
    class InstructionLatency {
    friend class PinInstrument;
  public:
    static const UINT32 MAX_INDEX = 4096;
    static const UINT32 INDEX_SPECIAL = 3000;
    static const UINT32 MAX_MEM_SIZE = 512;

  private:
    /// Construction of latency table follows the opcode generation function in
    /// $(PIN_ROOT)/source/tools/SimpleExamples/opcodemix.cpp
    static COST _instruction_latency[MAX_COST_SITE][MAX_INDEX];

  public:
    /// Default initialization.
    /// Initialize _instruction_latency with hard-coded instruction latency.
    InstructionLatency();

    /// Initialization with input config.
    InstructionLatency(const std::string filename);

    static VOID SetBBLSize(BBLID _BBL_size);

    /// Add up the cost of all instructions
    static VOID InstructionCount(UINT32 opcode, BOOL ismem);

    /// The instrumentation function for normal instructions
    static VOID InstructionInstrument(INS ins, VOID *v);

  public:
    /// Read instruction latency config to _instruction_latency from ofstream or file.
    /// Invalid values (including negative latency, non-integer values) will be ignored.
    static VOID ReadConfig(const std::string filename);

    /// Write the current instruction latency config to ofstream or file.
    /// If no modification is made, then this will output the 
    /// default instruction latency config PIMProf will use.
    static std::ostream& WriteConfig(std::ostream& out);
    static VOID WriteConfig(const std::string filename);
};

} // namespace PIMProf

#endif // __INSTRUCTIONLATENCY_H__