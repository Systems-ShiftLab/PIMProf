#ifndef __PINUTIL_H__
#define __PINUTIL_H__

#include "pin.H"
#include "INIReader.h"

namespace PIMProf {
    typedef UINT32 CACHE_STATS;
    typedef FLT64 COST;
    typedef UINT32 BBLID;

    static const UINT32 MAX_COST_SITE = 2;
    enum CostSite {
        CPU = 0,
        PIM = 1
    };
    static const std::string CostSiteName[MAX_COST_SITE];
}

#endif // __PINUTIL_H__