//===- PinInstrument.h - Utils for instrumentation --------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//


#ifndef __PININSTRUMENT_H__
#define __PININSTRUMENT_H__

#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <unistd.h>
#include <cmath>
#include <list>
#include <set>
#include "pin.H"

#include "../LLVMAnalysis/Common.h"
#include "PinUtil.h"
#include "CostPackage.h"
#include "Storage.h"
#include "Simulation.h"
#include "DataReuse.h"
#include "CostSolver.h"


namespace PIMProf {

class PinInstrument {
  private:
    CommandLineParser _command_line_parser;
    ConfigReader _config_reader;

    InstructionLatency _instruction_latency;
    MemoryLatency _memory_latency;
    CostSolver _cost_solver;

    STORAGE _storage;

    CostPackage _cost_package;


  public:
    PinInstrument() {}

  public:
    void initialize(int argc, char *argv[]);
    /// insert the instrumentation function before running simulation
    void instrument();

    /// run the actual simulation
    void simulate();
  
  public:
    void ReadControlFlowGraph(const std::string filename);
  
  // "self" enables us to use non-static members in static functions
  protected:
    static VOID DoAtAnnotatorHead(PinInstrument *self, ADDRINT bblid, ADDRINT isomp);
    static VOID DoAtAnnotatorTail(PinInstrument *self, ADDRINT bblid, ADDRINT isomp);

    /// The instrumentation function for an entire image
    static VOID ImageInstrument(IMG img, VOID *void_self);

    /// Finalization
    static VOID FinishInstrument(INT32 code, VOID *void_self);
};


} // namespace PIMProf


#endif // __PININSTRUMENT_H__
