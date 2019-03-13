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
    class InstructionLatency {
    public:
        vector<UINT32> latencytable;

    public:
        void InstructionLatency();

        void InstructionLatency(std::ofstream &);
    
    public:
        void ReadConfigFromFile();
        void WriteConfigToFile();

        /// Generate the default instruction latency config.
        /// This is the default instruction latency config PIMProf will use.
        void GenerateConfigTemplate();

        /// Generate a template of instruction latency config for the given program.
        /// This function will pre-execute the given program once.
        /// filename: the fstream of input program file.
        void GenerateConfigTemplate(const std::string &filename);

    };

    class PinInstrument {
    public:
        InstructionLatency latency;
    };
}

#endif // __PININSTRUMENT_H__