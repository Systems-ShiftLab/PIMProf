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
    static const std::string CostSiteName[MAX_COST_SITE] = { "CPU", "PIM" };

    static const BBLID GLOBALBBLID = -1;

    enum ACCESS_TYPE
    {
        ACCESS_TYPE_LOAD,
        ACCESS_TYPE_STORE,
        ACCESS_TYPE_NUM
    };

    typedef std::pair<BBLID, ACCESS_TYPE> BBLOP;
}

#endif // __PINUTIL_H__