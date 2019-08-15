//===- Common.h - Define name of annotators and files -----------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#ifndef __COMMON_H__
#define __COMMON_H__

static const std::string PIMProfAnnotatorFileName = "PIMProfAnnotator.bc";

static const std::string PIMProfAnnotatorHead = "PIMProfAnnotatorHead";
static const std::string PIMProfAnnotatorTail = "PIMProfAnnotatorTail";

// a hack for dealing with name mangling
static const std::string PIMProfOffloader = "_Z16PIMProfOffloaderii";

static const std::string PIMProfBBLIDMetadata = "basicblock.id";

static const std::string OpenMPIdentifier = ".omp_outlined";

static const int PIMProfAnnotatorBBLID = -1;

static const int BBLStartingID = 0;

#endif // __COMMON_H__