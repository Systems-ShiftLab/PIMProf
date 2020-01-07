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
    // void ReadControlFlowGraph(const std::string filename);
  
  // "self" enables us to use non-static members in static functions
  protected:
    /// ROI head and tail are used to mark out the beginning and end of the ROI,
    /// i.e., the region of instructions that we want to instrument.
    /// In other word, instructions outside the ROI will not call the analysis routine.
    static VOID DoAtROIHead(PinInstrument *self, THREADID threadid);
    static VOID DoAtROITail(PinInstrument *self, THREADID threadid);

    /// Annotation head and tail are used to mark out the beginning and end of a basic block.
    static VOID DoAtAnnotationHead(PinInstrument *self, ADDRINT bblhash_hi, ADDRINT bblhash_lo, ADDRINT isomp, THREADID threadid);
    static VOID DoAtAnnotationTail(PinInstrument *self, ADDRINT bblhash_hi, ADDRINT bblhash_lo, ADDRINT isomp, THREADID threadid);

    static VOID DoAtAcceleratorHead(PinInstrument *self);
    static VOID DoAtAcceleratorTail(PinInstrument *self);

    /// The instrumentation function for an entire image
    static VOID ImageInstrument(IMG img, VOID *void_self);

    /// The instrumentation function for magic instructions
    static VOID HandleMagicOP(PinInstrument *self, ADDRINT op, THREADID threadid);
    static VOID InstructionInstrument(INS ins, VOID *void_self);

    /// Execute when a new thread starts and ends
    static VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *void_self);
    static VOID ThreadFinish(THREADID threadid, const CONTEXT *ctxt, INT32 flags, VOID *void_self);

    /// Finalization
    static VOID FinishInstrument(INT32 code, VOID *void_self);
};


} // namespace PIMProf


#endif // __PININSTRUMENT_H__
