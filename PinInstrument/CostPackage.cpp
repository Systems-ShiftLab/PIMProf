//===- CostPackage.cpp - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#include "CostPackage.h"

using namespace PIMProf;

/* ===================================================================== */
/* CostPackage */
/* ===================================================================== */

void CostPackage::initialize(ConfigReader &reader)
{
    _inOpenMPRegion = false;
    _data_reuse.initialize(reader);
}