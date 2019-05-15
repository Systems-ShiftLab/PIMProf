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

    static const BBLID GLOBALBBLID = 0x7FFFFFFF;

    enum ACCESS_TYPE
    {
        ACCESS_TYPE_LOAD,
        ACCESS_TYPE_STORE,
        ACCESS_TYPE_NUM
    };

    typedef std::pair<BBLID, ACCESS_TYPE> BBLOP;

    inline INT32 INIErrorMsg(INT32 error, const string &filename, std::ostream &out) 
    {
        if (error == 0)
            return error;
        out << "################################################################################\n";
        out << "## ";
        if (error == -1) {
            out << "PIMProf: .ini file open error." << std::endl;
        }
        else if (error == -2) {
            out << "PIMProf: .ini file memory allocation for parsing error." << std::endl;
        } 
        
        else if (error > 0) {
            out << "PIMProf: .ini file parsing failure on line "
                << error 
                << "." << std::endl;
        }
        out << "## Filename: " << filename << std::endl;
        out << "################################################################################\n";
        return error;
    }
}

#endif // __PINUTIL_H__