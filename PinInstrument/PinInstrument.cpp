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
#include "./PinInstrument.h"

using namespace PIMProf;

INT32 Usage()
{
    cerr <<
        "This pin tool computes a static and dynamic opcode mix profile\n"
        "\n";

    cerr << KNOB_BASE::StringKnobSummary();

    cerr << endl;

    return -1;
}

void InstructionLatency::PrintConfigTemplate(ostream& out)
{
    for ( UINT32 i = 0; i < MAX_INDEX; i++)
    {
        std::string output = ljstr(OPCODE_StringShort(i), 15);
            if (output != "LAST") {
            out << setw(4) << i << " " << output << std::endl;
        }
    }
}

int main(int argc, CHAR *argv[])
{
    PIN_InitSymbols();

    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }

    auto l = InstructionLatency();
    l.PrintConfigTemplate(std::cout);

    PIN_StartProgram();
    return 0;
}

