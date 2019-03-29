//===- PinInstrument.h - Utils for instrumentation --------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//


#ifndef __PININSTRUMENT_H__
#define __PININSTRUMENT_H__

#include <stack>
#include <assert.h>
#include <list>
#include "pin.H"

#include "Cache.h"

namespace PIMProf {
    
    const UINT32 MAX_INDEX = 4096;
    // const UINT32 INDEX_SPECIAL = 3000;
    const UINT32 MAX_MEM_SIZE = 512;

    // const UINT32 INDEX_TOTAL =          INDEX_SPECIAL + 0;
    // const UINT32 INDEX_MEM_ATOMIC =     INDEX_SPECIAL + 1;
    // const UINT32 INDEX_STACK_READ =     INDEX_SPECIAL + 2;
    // const UINT32 INDEX_STACK_WRITE =    INDEX_SPECIAL + 3;
    // const UINT32 INDEX_IPREL_READ =     INDEX_SPECIAL + 4;
    // const UINT32 INDEX_IPREL_WRITE =    INDEX_SPECIAL + 5;
    // const UINT32 INDEX_MEM_READ_SIZE =  INDEX_SPECIAL + 6;
    // const UINT32 INDEX_MEM_WRITE_SIZE = INDEX_SPECIAL + 6 + MAX_MEM_SIZE;
    // const UINT32 INDEX_SPECIAL_END   =  INDEX_SPECIAL + 6 + MAX_MEM_SIZE + MAX_MEM_SIZE;

namespace ITLB
{
    // instruction TLB: 4 kB pages, 32 entries, fully associative
    const UINT32 lineSize = 4*KILO;
    const UINT32 cacheSize = 32 * lineSize;
    const UINT32 associativity = 32;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;

    const UINT32 max_sets = cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = associativity;

    typedef CACHE_LRU(max_sets, max_associativity, allocation) CACHE;
} // namespace PIMProf::ITLB

namespace DTLB
{
    // data TLB: 4 kB pages, 32 entries, fully associative
    const UINT32 lineSize = 4*KILO;
    const UINT32 cacheSize = 32 * lineSize;
    const UINT32 associativity = 32;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;

    const UINT32 max_sets = cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = associativity;

    typedef CACHE_LRU(max_sets, max_associativity, allocation) CACHE;
} // namespace PIMProf::DTLB

namespace IL1
{
    // 1st level instruction cache: 32 kB, 32 B lines, 32-way associative
    const UINT32 cacheSize = 32*KILO;
    const UINT32 lineSize = 32;
    const UINT32 associativity = 32;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_NO_ALLOCATE;

    const UINT32 max_sets = cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = associativity;

    typedef CACHE_LRU(max_sets, max_associativity, allocation) CACHE;
} // namespace PIMProf::IL1

namespace DL1
{
    // 1st level data cache: 32 kB, 32 B lines, 32-way associative
    const UINT32 cacheSize = 32*KILO;
    const UINT32 lineSize = 32;
    const UINT32 associativity = 32;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_NO_ALLOCATE;

    const UINT32 max_sets = cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = associativity;

    typedef CACHE_LRU(max_sets, max_associativity, allocation) CACHE;
} // namespace PIMProf::DL1

namespace UL2
{
    // 2nd level unified cache: 2 MB, 64 B lines
    const UINT32 cacheSize = 2*MEGA;
    const UINT32 lineSize = 64;
    const UINT32 associativity = 32;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;

    const UINT32 max_sets = cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = associativity;

    typedef CACHE_LRU(max_sets, max_associativity, allocation) CACHE;
} // namespace PIMProf::UL2

namespace UL3
{
    // 3rd level unified cache: 16 MB, 64 B lines, direct mapped
    const UINT32 cacheSize = 16*MEGA;
    const UINT32 lineSize = 64;
    const UINT32 associativity = 32;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;
    const UINT32 max_associativity = associativity;

    const UINT32 max_sets = cacheSize / (lineSize * associativity);

    typedef CACHE_LRU(max_sets, max_associativity, allocation) CACHE;
} // namespace PIMProf::UL3


class MemoryLatency {
    friend class PinInstrument;

  private:

  public:
    static VOID Ul2Access(ADDRINT addr, UINT32 size, CACHE_BASE::ACCESS_TYPE accessType);

    /// Do on instruction cache reference
    static VOID InsRef(ADDRINT addr);

    /// Do on multi-line data cache references
    static VOID MemRefMulti(ADDRINT addr, UINT32 size, CACHE_BASE::ACCESS_TYPE accessType);

    /// Do on a single-line data cache reference
    static VOID MemRefSingle(ADDRINT addr, UINT32 size, CACHE_BASE::ACCESS_TYPE accessType);

    /// The instrumentation function for memory instructions
    static VOID Instruction(INS ins, VOID *v);

    /// Finalization
    static VOID Fini(INT32 code, VOID * v);

  public:
    /// Read cache config to from ofstream or file.
    static VOID ReadConfig(const std::string filename);

    /// Write the current cache config to ofstream or file.
    /// If no modification is made, then this will output the 
    /// default cache config PIMProf will use.
    static VOID WriteConfig(std::ostream& out);
    static VOID WriteConfig(const std::string filename);
};

class InstructionLatency {
    friend class PinInstrument;

  private:
    /// Construction of latency table follows the opcode generation function in
    /// $(PIN_ROOT)/source/tools/SimpleExamples/opcodemix.cpp
    static UINT32 latencytable[MAX_INDEX];

  public:
    /// Default initialization.
    /// Initialize latencytable with hard-coded instruction latency.
    InstructionLatency();

    /// Initialization with input config.
    InstructionLatency(const std::string filename);

  public:
    /// Read instruction latency config to latencytable from ofstream or file.
    /// Invalid values (including negative latency, non-integer values) will be ignored.
    static VOID ReadConfig(const std::string filename);

    /// Write the current instruction latency config to ofstream or file.
    /// If no modification is made, then this will output the 
    /// default instruction latency config PIMProf will use.
    static VOID WriteConfig(std::ostream& out);
    static VOID WriteConfig(const std::string filename);
};

class PinInstrument {
  public:
    typedef std::stack<UINT32> BasicBlockIDStack;

  private:
    static MemoryLatency memory_latency;
    static InstructionLatency instruction_latency;
    static BasicBlockIDStack bblidstack;

  public:
    PinInstrument();

  public:
    static inline VOID DoAtAnnotatorHead(UINT32 BBLID)
    {
        std::cout << std::oct << "PIMProfHead: " << BBLID << std::endl;
        bblidstack.push(BBLID);
    }

    static inline VOID DoAtAnnotatorTail(UINT32 BBLID)
    {
        std::cout << std::oct << "PIMProfTail: " << BBLID << std::endl;
        if (bblidstack.top() != BBLID) {
            assert(0 && "Annotator head and tail does not match! This may be cause by exceptions or gotos in the original program.");
        }
        bblidstack.pop();
    }

    inline UINT32 GetCurrentBBL()
    {
        return bblidstack.top();
    }
};


} // namespace PIMProf


#endif // __PININSTRUMENT_H__