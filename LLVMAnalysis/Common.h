//===- Common.h - Define name of annotators and files -----------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#ifndef __COMMON_H__
#define __COMMON_H__

#include <string>

static const std::string PIMProfAnnotationHead = "PIMProfAnnotationHead";
static const std::string PIMProfAnnotationTail = "PIMProfAnnotationTail";

// a hack for dealing with name mangling
static const std::string PIMProfOffloaderName = "_Z16PIMProfOffloaderiiii";
static const std::string PIMProfOffloader2Name = "_Z17PIMProfOffloader2i";
static const std::string PIMProfOffloaderNullName = "_Z20PIMProfOffloaderNulliiii";

static const std::string PIMProfDecisionEnv = "PIMPROFDECISION";
static const std::string PIMProfROIEnv = "PIMPROFROI";
static const std::string PIMProfInjectModeEnv = "PIMPROFINJECTMODE";

static const std::string PIMProfBBLIDMetadata = "basicblock.id";

static const std::string OpenMPIdentifier = ".omp";

static const std::string PThreadsIdentifier = "pthread_create";

static const int PIMProfAnnotationBBLID = -1;

static const int BBLStartingID = 0;

enum MAGIC_OP {
    MAGIC_OP_ANNOTATIONHEAD,
    MAGIC_OP_ANNOTATIONTAIL,
    MAGIC_OP_ROIBEGIN,
    MAGIC_OP_ROIEND,
    MAGIC_OP_ROIDECISIONBEGIN,
    MAGIC_OP_ROIDECISIONEND
};

// We use the last i64 to encode control bits, layout:
// |    isomp    |   optype    |
// 64            32            0
class ControlValue {
  public:
    static inline uint64_t GetControlValue(uint64_t optype, uint64_t isomp) {
        return ((isomp << 32) | optype);
    }
    static inline uint64_t GetIsOpenMP(uint64_t controlvalue) {
        return (controlvalue >> 32);
    }
    static inline uint64_t GetOpType(uint64_t controlvalue) {
        return ((((uint64_t)1 << 32) - 1) & controlvalue);
    }
};





#endif // __COMMON_H__