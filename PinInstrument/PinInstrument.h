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

// #include "MemoryLatency.h"
#include "InstructionLatency.h"
// #include "DataReuse.h"
// #include "CostSolver.h"
// #include "Cache.h"

namespace PIMProf {

class PinInstrument {
  private:
    InstructionLatency _instruction_latency;
    // MemoryLatency memory_latency;
    // DataReuse data_reuse;
    // CostSolver solver;
    ConfigReader _config_reader;
    BBLScope _bbl_scope;
    bool _inOpenMPRegion;
    BBLID _bbl_size;

  public:
    PinInstrument() {}

  public:
    void initialize(int argc, char *argv[]);
    /// insert the instrumentation function before running simulation
    void instrument();

    /// run the actual simulation
    void simulate();

    inline BBLScope &getScope() {
        return _bbl_scope;
    }

    inline BBLID &getBBLSize() {
        return _bbl_size;
    }
  
  public:
    void ReadControlFlowGraph(const std::string filename);
  
  // "self" enables us to use non-static members in static functions
  protected:
    static VOID DoAtAnnotatorHead(PinInstrument *self, BBLID bblid, INT32 isomp);
    static VOID DoAtAnnotatorTail(PinInstrument *self, BBLID bblid, INT32 isomp);

    /// The instrumentation function for an entire image
    static VOID ImageInstrument(IMG img, VOID *void_self);

    /// Finalization
    static VOID FinishInstrument(INT32 code, VOID *void_self);
};


} // namespace PIMProf


#endif // __PININSTRUMENT_H__
