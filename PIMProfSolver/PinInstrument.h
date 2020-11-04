//===- PIMProfSolver.h - Utils for instrumentation --------------*- C++ -*-===//
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

#include "Common.h"
#include "Util.h"
#include "CostPackage.h"
#include "Storage.h"
#include "Simulation.h"
#include "DataReuse.h"
#include "CostSolver.h"
#include "../LLVMAnalysis/PIMProfAnnotation.h"


namespace PIMProf {

class PIMProfSolver {
  private:
    InstructionLatency _instruction_latency;
    MemoryLatency _memory_latency;
    CostSolver _cost_solver;

    STORAGE _storage;

    CostPackage _cost_package;


  public:
    PIMProfSolver() {}

  public:
    void initialize(int argc, char *argv[]);

    /// run the actual simulation
    void simulate();
  
  public:
    // void ReadControlFlowGraph(const std::string filename);
  
  // "self" enables us to use non-static members in static functions
  protected:
    /// [deprecated] The instrumentation function for an entire image
    // static void ImageInstrument(IMG img, void *void_self);

    /// Main entrance of magic instruction instrumentation
    static void InstructionInstrument(INS ins, void *void_self);
    static void HandleMagic(PIMProfSolver *self, ADDRINT bblhash_hi, ADDRINT bblhash_lo, ADDRINT control_value, THREADID threadid);

    /// Annotation head and tail are used to mark out the beginning and end of a basic block.
    static void DoAtAnnotationHead(PIMProfSolver *self, ADDRINT bblhash_hi, ADDRINT bblhash_lo, ADDRINT isomp, THREADID threadid);
    static void DoAtAnnotationTail(PIMProfSolver *self, ADDRINT bblhash_hi, ADDRINT bblhash_lo, ADDRINT isomp, THREADID threadid);

    /// ROI head and tail are used to mark out the beginning and end of the ROI,
    /// i.e., the region of instructions that we want to instrument.
    /// In other word, instructions outside the ROI will not call the analysis routine.
    static void DoAtROIHead(PIMProfSolver *self, THREADID threadid);
    static void DoAtROITail(PIMProfSolver *self, THREADID threadid);

    /// ROI decision head and tail are used to mark out by hand the regions that will be offloaded to PIM
    static void DoAtROIDecisionHead(PIMProfSolver *self, THREADID threadid);
    static void DoAtROIDecisionTail(PIMProfSolver *self, THREADID threadid);


    static void DoAtAcceleratorHead(PIMProfSolver *self);
    static void DoAtAcceleratorTail(PIMProfSolver *self);


    /// Execute when a new thread starts and ends
    static void ThreadStart(THREADID threadid, CONTEXT *ctxt, int32_t flags, void *void_self);
    static void ThreadFinish(THREADID threadid, const CONTEXT *ctxt, int32_t flags, void *void_self);

    /// Finalization
    static void FinishInstrument(int32_t code, void *void_self);
};


} // namespace PIMProf


#endif // __PININSTRUMENT_H__
