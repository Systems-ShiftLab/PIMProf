//===- PinInstrument.h - Utils for instrumentation --------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//


#ifndef __PININSTRUMENT_H__
#define __PININSTRUMENT_H__

#include "pin.H"

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


    class InstructionLatency {
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
        void ReadConfig(const std::string filename);

        /// Write the current instruction latency config to ofstream or file.
        /// If no modification is made, then this will output the 
        /// default instruction latency config PIMProf will use.
        void WriteConfig(std::ostream& out);
        void WriteConfig(const std::string filename);


    };

    class PinInstrument {
    public:
        InstructionLatency latency;
    };
}

#endif // __PININSTRUMENT_H__