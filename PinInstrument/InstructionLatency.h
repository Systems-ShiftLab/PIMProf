//===- InstructionLatency.h - Utils for instrumentation --------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//



#ifndef __INSTRUCTIONLATENCY_H__
#define __INSTRUCTIONLATENCY_H__

#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <unistd.h>
#include <cmath>
#include <stack>
#include <list>
#include <set>

#include "pin.H"
#include "../LLVMAnalysis/Common.h"
#include "PinUtil.h"

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
    COST _instruction_latency[MAX_COST_SITE][MAX_INDEX];

    std::vector<COST> _BBL_instruction_cost[MAX_COST_SITE];
    COST _instruction_multiplier[MAX_COST_SITE];

    long long int _instr_cnt;
    long long int _mem_instr_cnt;
    long long int _nonmem_instr_cnt;
    BBLScope *_bbl_scope;
    BBLID _bbl_size;

    int canary;

  public:
    InstructionLatency() {
        infomsg() << "InstructionLatency allocated" << std::endl;
    }
    ~InstructionLatency() {
        infomsg() << "InstructionLatency deallocated" << std::endl;
    }
    /// Default initialization.
    /// Initialize _instruction_latency with hard-coded instruction latency.
    void initialize(BBLScope *scope, BBLID bbl_size);

    /// Initialization with input config.
    void initialize(BBLScope *scope, BBLID bbl_size, ConfigReader &reader);

    VOID SetBBLSize(BBLID bbl_size);

  public:
    /// insert the instrumentation function before running simulation
    void instrument();

  public:
    /// Read instruction latency config to _instruction_latency from config_reader.
    /// Invalid values (including negative latency, non-integer values) will be ignored.
    VOID ReadConfig(ConfigReader &reader);

    /// Write the current instruction latency config to ofstream or file.
    /// If no modification is made, then this will output the 
    /// default instruction latency config PIMProf will use.
    std::ostream& WriteConfig(std::ostream& out);
    VOID WriteConfig(const std::string filename);

  protected:
  /// Add up the cost of all instructions
  static VOID InstructionCount(InstructionLatency *self, UINT32 opcode, BOOL ismem);

    /// The instrumentation function for normal instructions
  static VOID InstructionInstrument(INS ins, VOID *void_self);

};

} // namespace PIMProf

#endif // __INSTRUCTIONLATENCY_H__