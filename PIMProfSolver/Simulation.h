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
#include "Common.h"

#include "Util.h"
#include "CostPackage.h"
#include "Storage.h"

namespace PIMProf {
class InstructionLatency {
    friend class PIMProfSolver;

  private:

    /// Reference to PIMProfSolver data
    CostPackage *_cost_package;

  public:
    /// Default initialization.
    /// Initialize _instruction_latency with hard-coded instruction latency.
    void initialize(CostPackage *cost_package);

    /// Initialization with input config.
    void initialize(CostPackage *cost_package, ConfigReader &reader);

    // void SetBBLSize(BBLID bbl_size);

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
  static void InstructionCount(InstructionLatency *self, uint32_t opcode, bool ismem, uint32_t simd_len, THREADID threadid);

};


class MemoryLatency {
    friend class PIMProfSolver;
  private:
    STORAGE *_storage;
    /// Reference to PIMProfSolver data
    CostPackage *_cost_package;

  public:
    void initialize(STORAGE *cache, CostPackage *cost_package, ConfigReader &reader);

  public:
    /// Read cache config from ofstream or file.
    void ReadConfig(ConfigReader &reader);

    // void SetBBLSize(BBLID bbl_size);

  protected:

    /// Do on instruction cache reference
    static void InstrCacheRef(MemoryLatency *self, ADDRINT addr, uint32_t size, uint32_t simd_len, THREADID threadid);

    /// Do on data cache reference
    static void DataCacheRef(MemoryLatency *self, ADDRINT ip, ADDRINT addr, uint32_t size, ACCESS_TYPE accessType, uint32_t simd_len, THREADID threadid);
};

} // namespace PIMProf

#endif // __SIMULATION_H__