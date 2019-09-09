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

/* ===================================================================== */
/* main */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    PinInstrument::instance().initialize(argc, argv);
    PinInstrument::instance().simulate();
    return 0;
}