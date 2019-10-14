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

void CostPackage::initialize()
{
    _inOpenMPRegion.resize(_bbl_size);
    for (UINT32 i = 0; i < _bbl_size; i++) {
        _inOpenMPRegion[i] = false;
    }
}