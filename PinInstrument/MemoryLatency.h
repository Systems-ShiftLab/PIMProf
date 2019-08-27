#ifndef __MEMORYLATENCY_H__
#define __MEMORYLATENCY_H__

#include "pin.H"

#include "PinUtil.h"
#include "Cache.h"

namespace PIMProf {
class MemoryLatency {
    friend class PinInstrument;

  private:
    static CACHE cache;

  public:

    static VOID SetBBLSize(BBLID _BBL_size);

    /// Do on instruction cache reference
    static VOID InstrCacheRef(ADDRINT addr);

    /// Do on multi-line data cache references
    static VOID DataCacheRefMulti(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);

    /// Do on a single-line data cache reference
    static VOID DataCacheRefSingle(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);

    /// The instrumentation function for memory instructions
    static VOID InstructionInstrument(INS ins, VOID *v);

    /// Finalization
    static VOID FinishInstrument(INT32 code, VOID * v);

  public:
    /// Read cache config from ofstream or file.
    static VOID ReadConfig(const std::string filename);

    /// Write the current cache config to ofstream or file.
    /// If no modification is made, then this will output the 
    /// default cache config PIMProf will use.
    static std::ostream& WriteConfig(std::ostream& out);
    static VOID WriteConfig(const std::string filename);
};

} // namespace PIMProf

#endif // __MEMORYLATENCY_H__