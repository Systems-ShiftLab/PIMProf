/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2018 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/*! @file
 *  This file contains a static and dynamic opcode  mix profiler
 */

#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <unistd.h>
#include "control_manager.H"

#include "../LLVMAnalysis/Common.h"
#include "PinInstrument.h"

using namespace CONTROLLER;
using namespace PIMProf;

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,         "pintool",
    "o", "opcodemix.out", "specify profile file name");
KNOB<BOOL>   KnobPid(KNOB_MODE_WRITEONCE,                "pintool",
    "i", "0", "append pid to output");
KNOB<BOOL>   KnobProfilePredicated(KNOB_MODE_WRITEONCE,  "pintool",
    "p", "0", "enable accurate profiling for predicated instructions");
KNOB<BOOL>   KnobProfileStaticOnly(KNOB_MODE_WRITEONCE,  "pintool",
    "s", "0", "terminate after collection of static profile for main image");
#ifndef TARGET_WINDOWS
KNOB<BOOL>   KnobProfileDynamicOnly(KNOB_MODE_WRITEONCE, "pintool",
    "d", "0", "Only collect dynamic profile");
#else
KNOB<BOOL>   KnobProfileDynamicOnly(KNOB_MODE_WRITEONCE, "pintool",
    "d", "1", "Only collect dynamic profile");
#endif
KNOB<BOOL>   KnobNoSharedLibs(KNOB_MODE_WRITEONCE,       "pintool",
    "no_shared_libs", "0", "do not instrument shared libraries");


int main(int argc, CHAR *argv[])
{
    PIN_InitSymbols();

    if( PIN_Init(argc,argv) )
    {
        ASSERTX(0);
    }
    
    IMG_AddInstrumentFunction(PinInstrument::Image, 0);

    MemoryLatency::ReadConfig("/home/warsier/Documents/PIMProf/PinInstrument/defaultcache.ini");
    CostSolver::AddControlCost("/home/warsier/Documents/PIMProf/TestCase/merged.out");
    INS_AddInstrumentFunction(MemoryLatency::Instruction, 0);
    PIN_AddFiniFunction(MemoryLatency::Fini, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
