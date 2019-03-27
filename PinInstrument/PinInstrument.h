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
#include "pin.H"

typedef UINT32 CACHE_STATS;

#include "pin_cache.H"

namespace CACHE_SET {
    template <UINT32 MAX_ASSOCIATIVITY = 4>
    class LRU {
    public:
        std::list<CACHE_TAG> CacheTagList;
    private:
        // this is a fixed-size list where the size is the current associativity
        // front is MRU, back is LRU
        CacheTagList _tags;
    public:
        LRU(UINT32 associativity = MAX_ASSOCIATIVITY)
        {
            ASSERTX(associativity <= MAX_ASSOCIATIVITY);
            for (INT32 i = 0; i < associativity; i++) {
                _tags.push_back(CACHE_TAG(0));
            }
        }

        VOID SetAssociativity(UINT32 associativity)
        {
            ASSERTX(associativity <= MAX_ASSOCIATIVITY);
            _tags.clear();
            for (INT32 i = 0; i < associativity; i++) {
                _tags.push_back(CACHE_TAG(0));
            }
        }

        UINT32 GetAssociativity(UINT32 associativity) 
        {
            return _tags.size();
        }

        UINT32 Find(CACHE_TAG tag)
        {
            for (CacheTagList::iterator it = _tags.begin(), CacheTagList::iterator eit = _tags.end(); it != eit; it++) {
                // promote the accessed cache line to the front
                if (*it == tag) {
                    _tags.erase(it);
                    _tags.push_front(tag);
                    return true;
                }
            }
            return false;
        }

        VOID Replace(CACHE_TAG tag)
        {
            _tags.pop_back();
            _tags.push_front(tag);
        }

        VOID Flush()
        {
            UINT32 associativity = _tags.size();
            _tags.clear();
            for (INT32 i = 0; i < associativity; i++) {
                _tags.push_back(CACHE_TAG(0));
            }
        }
    };
} // namespace CACHE_SET

#define CACHE_LRU(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::LRU<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>

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
    }

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
    }

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
    }

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
    }

    namespace UL2
    {
        // 2nd level unified cache: 2 MB, 64 B lines, direct mapped
        const UINT32 cacheSize = 2*MEGA;
        const UINT32 lineSize = 64;
        const UINT32 associativity = 1;
        const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;

        const UINT32 max_sets = cacheSize / (lineSize * associativity);

        typedef CACHE_DIRECT_MAPPED(max_sets, allocation) CACHE;
    }

    namespace UL3
    {
        // 3rd level unified cache: 16 MB, 64 B lines, direct mapped
        const UINT32 cacheSize = 16*MEGA;
        const UINT32 lineSize = 64;
        const UINT32 associativity = 1;
        const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;

        const UINT32 max_sets = cacheSize / (lineSize * associativity);

        typedef CACHE_DIRECT_MAPPED(max_sets, allocation) CACHE;
    }

    class InstructionLatency {
        friend class PinInstrument;
    
    private:
        /// Construction of latency table follows the opcode generation function in
        /// $(PIN_ROOT)/source/tools/SimpleExamples/opcodemix.cpp
        UINT32 latencytable[MAX_INDEX];

    public:
        /// Default initialization.
        /// Initialize latencytable with hard-coded instruction latency.
        InstructionLatency();

        /// Initialization with input config.
        InstructionLatency(const std::string filename);

    public:
        /// Read instruction latency config to latencytable from ofstream or file.
        /// Invalid values (including negative latency, non-integer values) will be ignored.
        VOID ReadConfig(const std::string filename);

        /// Write the current instruction latency config to ofstream or file.
        /// If no modification is made, then this will output the 
        /// default instruction latency config PIMProf will use.
        VOID WriteConfig(std::ostream& out);
        VOID WriteConfig(const std::string filename);


    };

    class PinInstrument {
    public:
        typedef std::stack<UINT32> BasicBlockIDStack;
    
    private:
        InstructionLatency latency;
        BasicBlockIDStack bblidstack;
    
    public:
        PinInstrument();

    public:
        inline VOID DoAtAnnotatorHead(UINT32 BBLID)
        {
            bblidstack.push(BBLID);
        }

        inline VOID DoAtAnnotatorTail(UINT32 BBLID)
        {
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

    class MemoryLatency {
    public:
        static VOID InsRef(ADDRINT addr);
        static VOID MemRefMulti(ADDRINT addr, UINT32 size, CACHE_BASE::ACCESS_TYPE accessType);
        static VOID MemRefSingle(ADDRINT addr, UINT32 size, CACHE_BASE::ACCESS_TYPE accessType);
        static VOID Instruction(INS ins, VOID *v);
    };

} // namespace PIMProf


#endif // __PININSTRUMENT_H__