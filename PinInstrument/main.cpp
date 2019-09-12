//===- main.cpp - The main entrance of PinInstrument tool ---------------------------*- C++ -*-===//
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
#include "control_manager.H"

// #include "../LLVMAnalysis/Common.h"
#include "pin.H"
#include "PinUtil.h"
#include "PinInstrument.h"


using namespace CONTROLLER;
using namespace PIMProf;

// PinInstrument instance has to be a global variable so that
// it can still stay alive when we are out of the scope of main()
// and in the scope of Pin.
PinInstrument instance;

/* ===================================================================== */
/* main */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    instance.initialize(argc, argv);
    instance.simulate();
    cout << "wowow" << std::endl;
    return 0;
}