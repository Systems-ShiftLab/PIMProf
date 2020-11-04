//===- PinUtil.h - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#ifndef __PINUTIL_H__
#define __PINUTIL_H__

#include <stack>
#include <algorithm>
#include <iostream>
#include <bitset>
#include <cassert>
#include <cstdarg>

#include "../LLVMAnalysis/Common.h"
#include "INIReader.h"

namespace PIMProf {
/* ===================================================================== */
/* Typedefs and constants */
/* ===================================================================== */

// MAX_LEVEL comes last to indicate the number of entries in this enum
enum StorageLevel {
    IL1, DL1, UL2, UL3, MEM, MAX_LEVEL
};
extern const std::string StorageLevelName[MAX_LEVEL];

/* ===================================================================== */
/* Typedefs and constants */
/* ===================================================================== */

const BBLID GLOBAL_BBLID = -1;

enum CostSite
{
    CPU,
    PIM,
    MAX_COST_SITE,
    INVALID = 0x3fffffff // a placeholder that does not count as a cost site
};
extern const std::string CostSiteName[MAX_COST_SITE];

enum ACCESS_TYPE
{
    ACCESS_TYPE_LOAD,
    ACCESS_TYPE_STORE,
    MAX_ACCESS_TYPE
};

/* ===================================================================== */
/* BBLScope */
/* ===================================================================== */
class BBLScope {
  private:
    std::stack<BBLID> bblidstack;
  public:
    inline BBLScope()
    {
        // we set the global BBL ID as 0
        bblidstack.push(GLOBAL_BBLID);
    }
    inline void push(BBLID bblid)
    {
        bblidstack.push(bblid);
    }
    inline void pop()
    {
        bblidstack.pop();
    }
    inline BBLID top()
    {
        return bblidstack.top();
    }
};

/* ===================================================================== */
/* Output Formatting */
/* ===================================================================== */

const std::string REDCOLOR = "\033[0;31m";
const std::string GREENCOLOR = "\033[0;32m";
const std::string YELLOWCOLOR = "\033[0;33m";
const std::string NOCOLOR = "\033[0m";

#define PRETTY_PRINT_FUNC_HELPER(TYPE, COLOR) \
inline int TYPE##msg(const char *format, ...) \
{ \
    va_list args; \
    va_start(args, format); \
    std::string output = COLOR + "## PIMProf " + #TYPE + ": " + NOCOLOR + std::string(format) + "\n"; \
    int result = vprintf(output.c_str(), args); \
    va_end(args); \
    return result; \
}

PRETTY_PRINT_FUNC_HELPER(info, GREENCOLOR)
PRETTY_PRINT_FUNC_HELPER(error, REDCOLOR)
PRETTY_PRINT_FUNC_HELPER(warning, YELLOWCOLOR)

/* ===================================================================== */
/* CommandLineParser */
/* ===================================================================== */

class CommandLineParser {
  public:
    enum Mode {
        MPKI, PARA, REUSE
    };
  private:
    std::string _cpustatsfile, _pimstatsfile;
    std::string _reusefile;
    std::string _outputfile;
    Mode _mode;

  public:
    void initialize(int argc, char *argv[]);

    inline std::string cpustatsfile() { return _cpustatsfile; }
    inline std::string pimstatsfile() { return _pimstatsfile; }
    inline std::string reusefile() { return _reusefile; }
    inline std::string outputfile() { return _outputfile; }
    inline Mode mode() { return _mode; }
    inline bool enableglobalbbl() { return true; } // whether considering the dependency with the global BBL, for debug use

};

/* ===================================================================== */
/* ConfigReader */
/* ===================================================================== */

class ConfigReader: public INIReader {
  private:
    std::string filename;
  public:
    inline ConfigReader() {}

    inline ConfigReader(std::string f)
        : INIReader(f), filename(f)
    {
        // int32_t error = ParseError();
        // if (error == -1) {
        //     errormsg() << ".ini file: Open error." << std::endl;
        // }
        // else if (error == -2) {
        //     errormsg() << ".ini file: Memory allocation error." << std::endl;
        // } 
        
        // else if (error > 0) {
        //     errormsg() << ".ini file: Parsing failure on line "
        //         << error 
        //         << "." << std::endl;
        // }
        // if (error) {
        //     errormsg() << "Filename: " << filename << std::endl;
        //     assert(0);
        // }
    }
};

/* ===================================================================== */
/* InstructionPrinter */
/* ===================================================================== */

void PrintInstruction(std::ostream *out, uint64_t insAddr, std::string insDis, uint32_t simd_len);
void PrintInfo(std::ostream *out, std::string info);

}// namespace PIMProf

#endif // __PINUTIL_H__
