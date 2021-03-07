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
#include <vector>
#include <map>
#include <unordered_map>

namespace PIMProf {
/* ===================================================================== */
/* Identifiers */
/* ===================================================================== */
static const std::string HORIZONTAL_LINE(60, '=');

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

/* ===================================================================== */
/* Typedefs */
/* ===================================================================== */

typedef uint32_t CACHE_STATS;
typedef double COST;
typedef int64_t BBLID;
typedef std::pair<uint64_t, uint64_t> UUID;

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

class UUIDHashFunc
{
public:
    // assuming UUID is already murmurhash-ed.
    std::size_t operator()(const UUID &key) const
    {
        size_t result = key.first ^ key.second;
        return result;
    }
};

template<typename val>
using UUIDHashMap = std::unordered_map<UUID, val, UUIDHashFunc>;
// template<typename val>
// using UUIDHashMap = std::map<UUID, val>;

/* ===================================================================== */
/* Enums and constants */
/* ===================================================================== */

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

/*
    Meaning of different mode:
    "CPU" or "CPUONLY" - Put start and end annotations around CPU-friendly BBLs
    "PIM" or "PIMONLY" - Put start and end annotations around PIM-friendly BBLs
    "NOTPIM" - Put end annotations at the start of PIM-friendly BBLs, and put start annotations at the end of PIM-friendly BBLs
    "ALL" - Put start and end annotations at all BBLs, in this case the second argument can be used to differentiate whether it is CPU or PIM friendly
*/
enum CostSite {
    CPU, PIM, MAX_COST_SITE,
    NOTPIM, ALL,
    DEFAULT = 0x0fffffff,
    INVALID = 0x3fffffff // a placeholder that does not count as a cost site
};

const std::string CostSiteString[] {
    "C", "P", "M",
    "N", "A",
    "D",
    "I"
};

inline const std::string getCostSiteString(CostSite site)
{
    return CostSiteString[site];
}

/// A DECISION is a vector that represents a certain offloading decision, for example:
/// A DECISION vector (PIM, CPU, CPU, PIM) means:
/// put the 1st and 4th BBL on PIM and 2nd and 3rd on CPU for execution
/// The target of CostSolver is to figure out the decision that will lead to the minimum total cost.
typedef std::vector<CostSite> DECISION;

enum ACCESS_TYPE
{
    ACCESS_TYPE_LOAD,
    ACCESS_TYPE_STORE,
    MAX_ACCESS_TYPE
};

enum class InjectMode {
    SNIPER, SNIPER2, VTUNE, PIMPROF,
    DEFAULT = 0x0fffffff,
    INVALID = 0x3fffffff // a placeholder that does not count as a cost site
};

// These numbers need to be consistent with Sniper
const int SNIPER_SIM_CMD_ROI_START = 1;
const int SNIPER_SIM_CMD_ROI_END = 2;

const int SNIPER_SIM_PIMPROF_BBL_START = 1024;
const int SNIPER_SIM_PIMPROF_BBL_END = 1025;
const int SNIPER_SIM_PIMPROF_OFFLOAD_START = 1026;
const int SNIPER_SIM_PIMPROF_OFFLOAD_END = 1027;

const int PIMPROF_DECISION_CPU = 0;
const int PIMPROF_DECISION_PIM = 1;

const BBLID GLOBAL_BBLID = 0;
const UUID GLOBAL_BBLHASH(GLOBAL_BBLID, GLOBAL_BBLID);
const BBLID MAIN_BBLID = 1;
const UUID MAIN_BBLHASH(MAIN_BBLID, MAIN_BBLID);

} // namespace PIMProf

#endif // __COMMON_H__