//===- PrintConfigTemplate.cpp ----------------------------------*- C++ -*-===//
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

#include "../LLVMAnalysis/Common.h"
#include "PinInstrument.h"

using namespace PIMProf;


int main(int argc, CHAR *argv[])
{
    PIN_InitSymbols();

    // if( PIN_Init(argc,argv) )
    // {
    //     return -1;
    // }
    InstructionLatency l;
    l.WriteConfig("output.ini");
    l.ReadConfig("output.ini");

    return 0;
}

