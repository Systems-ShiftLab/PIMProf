//===- Common.h - Define name of annotators and files -----------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#ifndef __COMMON_H__
#define __COMMON_H__

static const std::string PIMProfAnnotationHead = "PIMProfAnnotationHead";
static const std::string PIMProfAnnotationTail = "PIMProfAnnotationTail";

// a hack for dealing with name mangling
static const std::string PIMProfOffloader = "_Z16PIMProfOffloaderii";

static const std::string PIMProfBBLIDMetadata = "basicblock.id";

static const std::string OpenMPIdentifier = ".omp_outlined";

static const std::string PThreadsIdentifier = "pthread_create";

static const int PIMProfAnnotationBBLID = -1;

static const int BBLStartingID = 0;

#endif // __COMMON_H__