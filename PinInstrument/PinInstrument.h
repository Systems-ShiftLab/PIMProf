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

#include "MemoryLatency.h"
#include "InstructionLatency.h"
#include "DataReuse.h"
#include "CostSolver.h"
#include "Cache.h"

namespace PIMProf {

class PinInstrument {
  private:
    static MemoryLatency memory_latency;
    static InstructionLatency instruction_latency;
    static DataReuse data_reuse;
    static CostSolver solver;
    static BBLScope bbl_scope;
    

  public:
    PinInstrument() {};

  public:
    static VOID DoAtAnnotatorHead(BBLID bblid, INT32 isomp);
    static VOID DoAtAnnotatorTail(BBLID bblid, INT32 isomp);

    /// The instrumentation function for an entire image
    static VOID ImageInstrument(IMG img, VOID *v);

    /// Finalization
    static VOID FinishInstrument(INT32 code, VOID *v);
};


} // namespace PIMProf


#endif // __PININSTRUMENT_H__
