//===- InstructionLatency.h - Utils for instrumentation --------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//



#ifndef __SIMULATION_H__
#define __SIMULATION_H__

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
#include "CostPackage.h"
#include "Cache.h"

namespace PIMProf {
class InstructionLatency {
    friend class PinInstrument;

  private:

    /// Reference to PinInstrument data
    CostPackage *_cost_package;

  public:
    /// Default initialization.
    /// Initialize _instruction_latency with hard-coded instruction latency.
    void initialize(CostPackage *cost_package);

    /// Initialization with input config.
    void initialize(CostPackage *cost_package, ConfigReader &reader);

    void SetBBLSize(BBLID bbl_size);

  public:
    /// insert the instrumentation function before running simulation
    void instrument();

  public:
    /// Read instruction latency config to _instruction_latency from config_reader.
    /// Invalid values (including negative latency, non-integer values) will be ignored.
    void ReadConfig(ConfigReader &reader);

    /// Write the current instruction latency config to ofstream or file.
    /// If no modification is made, then this will output the 
    /// default instruction latency config PIMProf will use.
    std::ostream& WriteConfig(std::ostream& out);
    void WriteConfig(const std::string filename);

  protected:
  /// Add up the cost of all instructions
  static VOID InstructionCount(InstructionLatency *self, UINT32 opcode, BOOL ismem);

    /// The instrumentation function for normal instructions
  static VOID InstructionInstrument(INS ins, VOID *void_self);

};


class MemoryLatency {
  private:
    CACHE *_cache;
    /// Reference to PinInstrument data
    CostPackage *_cost_package;

  public:
    void initialize(CACHE *cache, CostPackage *cost_package, ConfigReader &reader);
    void instrument();

  public:
    /// Read cache config from ofstream or file.
    void ReadConfig(ConfigReader &reader);

    void SetBBLSize(BBLID bbl_size);

  protected:

    /// Do on instruction cache reference
    static VOID InstrCacheRef(MemoryLatency *self, ADDRINT addr);

    /// Do on multi-line data cache references
    static VOID DataCacheRefMulti(MemoryLatency *self, ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);

    /// Do on a single-line data cache reference
    static VOID DataCacheRefSingle(MemoryLatency *self, ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);

    /// The instrumentation function for memory instructions
    static VOID InstructionInstrument(INS ins, VOID *void_self);

    /// Finalization
    static VOID FinishInstrument(INT32 code, VOID * void_self);
};

} // namespace PIMProf

#endif // __SIMULATION_H__