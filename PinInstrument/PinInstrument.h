//===- PinInstrument.h - Utils for instrumentation --------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//


#ifndef __PININSTRUMENT_H__
#define __PININSTRUMENT_H__

#include <list>
#include <set>
#include "pin.H"

#include "PinUtil.h"

// #include "MemoryLatency.h"
// #include "InstructionLatency.h"
// #include "DataReuse.h"
// #include "CostSolver.h"
// #include "Cache.h"

namespace PIMProf {

class PinInstrument {
  private:
    // InstructionLatency instruction_latency;
    // MemoryLatency memory_latency;
    // DataReuse data_reuse;
    // CostSolver solver;
    ConfigReader config_reader;
    BBLScope bbl_scope;
    bool inOpenMPRegion;

  // PinInstrument is a singleton class
  private:
    PinInstrument() {}
  public:
    PinInstrument(PinInstrument const &) = delete;
    PinInstrument& operator = (PinInstrument const &) = delete;
    static PinInstrument &instance()
    {
        static PinInstrument inst;
        return inst;
    }

  public:
    int initialize(int argc, char *argv[]);
    void simulate();
  
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
