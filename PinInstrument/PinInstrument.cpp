//===- PinInstrument.cpp - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <unistd.h>

#include "pin.H"

#include "../LLVMAnalysis/Common.h"
#include "PinInstrument.h"

#include "INIReader.h"

using namespace PIMProf;

InstructionLatency::InstructionLatency()
{
}

void InstructionLatency::ReadConfig(const std::string filename)
{
    INIReader reader(filename);
    for (UINT32 i = 0; i < MAX_INDEX; i++)
    {
        std::string opcodestr = OPCODE_StringShort(i);
        if (opcodestr != "LAST") {
            istringstream buf(reader.Get("InstructionLatency", opcodestr, "1"));
            buf >> latencytable[i];
            std::cout << opcodestr << latencytable[i] << std::endl;
        }
    }
}

void InstructionLatency::WriteConfig(ostream& out)
{
    out << "[InstructionLatency]" << std::endl
        << "; <Instuction Name> = <Instruction Latency>" << std::endl;
    for (UINT32 i = 0; i < MAX_INDEX; i++)
    {
        std::string opcodestr = OPCODE_StringShort(i);
        if (opcodestr != "LAST") {
            opcodestr = ljstr(opcodestr, 15);
            out << opcodestr << "= " << latencytable[i] << std::endl;
        }
    }
}

void InstructionLatency::WriteConfig(const std::string filename)
{
    ofstream out;
    out.open(filename.c_str(), ios_base::out);
    WriteConfig(out);
    out.close();
}

