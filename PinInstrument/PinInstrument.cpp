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
#include <unistd.h>
#include "pin.H"
#include "control_manager.H"

#include "../LLVMAnalysis/Common.h"
#include "PinInstrument.h"

using namespace PIMProf;

InstructionLatency::InstructionLatency()
{

}

void InstructionLatency::PrintConfigTemplate(ostream& out)
{
    for ( UINT32 i = 0; i < MAX_INDEX; i++)
    {
        std::string output = OPCODE_StringShort(i);

        if (output != "LAST") {
            output = ljstr(output, 15);
            out << setw(4) << i << " " << output << std::endl;
        }
    }
}

