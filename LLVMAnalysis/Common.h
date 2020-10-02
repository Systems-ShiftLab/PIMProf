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

static const std::string VTuneOffloaderName = "_Z14VTuneOffloaderi";

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

enum VTUNE_MODE {
    VTUNE_MODE_CREATE,
    VTUNE_MODE_RESUME,
    VTUNE_MODE_PAUSE,
    VTUNE_MODE_DETACH,
    VTUNE_MODE_FRAME_BEGIN,
    VTUNE_MODE_FRAME_END
};

#define SNIPER_SIM_CMD_ROI_START       1
#define SNIPER_SIM_CMD_ROI_END         2

#define SNIPER_SIM_PIM_OFFLOAD_START  15
#define SNIPER_SIM_PIM_OFFLOAD_END    16


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

/*
    Meaning of different mode:
    "CPU" or "CPUONLY" - Put start and end annotations around CPU-friendly BBLs
    "PIM" or "PIMONLY" - Put start and end annotations around PIM-friendly BBLs
    "NOTPIM" - Put end annotations at the start of PIM-friendly BBLs, and put start annotations at the end of PIM-friendly BBLs
    "ALL" - Put start and end annotations at all BBLs, in this case the second argument can be used to differentiate whether it is CPU or PIM friendly
*/
enum class CallSite {
    CPU, PIM, MAX_COST_SITE,
    NOTPIM, ALL,
    DEFAULT = 0x0fffffff,
    INVALID = 0x3fffffff // a placeholder that does not count as a cost site
};

enum class InjectMode {
    SNIPER, VTUNE, PIMPROF,
    DEFAULT = 0x0fffffff,
    INVALID = 0x3fffffff // a placeholder that does not count as a cost site
};

typedef std::pair<uint64_t, uint64_t> UUID;

struct Decision {
    CallSite decision;
    int bblid;
    double difference;
    int parallel;
};

#endif // __COMMON_H__