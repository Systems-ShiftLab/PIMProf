//===- PinInstrument.h - Utils for instrumentation --------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//


#ifndef __PININSTRUMENT_H__
#define __PININSTRUMENT_H__

#include <stack>
#include <list>
#include <map>
#include "pin.H"

#include "PinUtil.h"
#include "Cache.h"

namespace PIMProf {


class MemoryLatency {
    friend class PinInstrument;

  private:
    static CACHE cache;

  public:

    /// Do on instruction cache reference
    static VOID InsRef(ADDRINT addr);

    /// Do on multi-line data cache references
    static VOID MemRefMulti(ADDRINT addr, UINT32 size, CACHE_LEVEL_BASE::ACCESS_TYPE accessType);

    /// Do on a single-line data cache reference
    static VOID MemRefSingle(ADDRINT addr, UINT32 size, CACHE_LEVEL_BASE::ACCESS_TYPE accessType);

    /// The instrumentation function for memory instructions
    static VOID Instruction(INS ins, VOID *v);

    /// Finalization
    static VOID Fini(INT32 code, VOID * v);

  public:
    /// Read cache config from ofstream or file.
    static VOID ReadConfig(const std::string filename);

    /// Write the current cache config to ofstream or file.
    /// If no modification is made, then this will output the 
    /// default cache config PIMProf will use.
    static std::ostream& WriteConfig(std::ostream& out);
    static VOID WriteConfig(const std::string filename);
};

class InstructionLatency {
    friend class PinInstrument;
  public:
    static const UINT32 MAX_INDEX = 4096;
    static const UINT32 INDEX_SPECIAL = 3000;
    static const UINT32 MAX_MEM_SIZE = 512;

    static const UINT32 INDEX_TOTAL =          INDEX_SPECIAL + 0;
    static const UINT32 INDEX_MEM_ATOMIC =     INDEX_SPECIAL + 1;
    static const UINT32 INDEX_STACK_READ =     INDEX_SPECIAL + 2;
    static const UINT32 INDEX_STACK_WRITE =    INDEX_SPECIAL + 3;
    static const UINT32 INDEX_IPREL_READ =     INDEX_SPECIAL + 4;
    static const UINT32 INDEX_IPREL_WRITE =    INDEX_SPECIAL + 5;
    static const UINT32 INDEX_MEM_READ_SIZE =  INDEX_SPECIAL + 6;
    static const UINT32 INDEX_MEM_WRITE_SIZE = INDEX_SPECIAL + 6 + MAX_MEM_SIZE;
    static const UINT32 INDEX_SPECIAL_END   =  INDEX_SPECIAL + 6 + MAX_MEM_SIZE + MAX_MEM_SIZE;

  private:
    /// Construction of latency table follows the opcode generation function in
    /// $(PIN_ROOT)/source/tools/SimpleExamples/opcodemix.cpp
    static UINT32 latencytable[MAX_INDEX];

  public:
    /// Default initialization.
    /// Initialize latencytable with hard-coded instruction latency.
    InstructionLatency();

    /// Initialization with input config.
    InstructionLatency(const std::string filename);

  // public:
  //   static BOOL IsMemReadIndex(UINT32 i);
  //   static BOOL IsMemWriteIndex(UINT32 i);
  //   static UINT32 INS_GetIndex(INS ins);
  //   static UINT32 IndexStringLength(BBL bbl, BOOL memory_acess_profile);
  //   static UINT32 MemsizeToIndex(UINT32 size, BOOL write);
  //   static UINT16 *INS_GenerateIndexString(INS ins, UINT16 *stats, BOOL memory_acess_profile);
  //   static string IndexToOpcodeString(UINT32 index);

  public:
    /// Read instruction latency config to latencytable from ofstream or file.
    /// Invalid values (including negative latency, non-integer values) will be ignored.
    static VOID ReadConfig(const std::string filename);

    /// Write the current instruction latency config to ofstream or file.
    /// If no modification is made, then this will output the 
    /// default instruction latency config PIMProf will use.
    static std::ostream& WriteConfig(std::ostream& out);
    static VOID WriteConfig(const std::string filename);
};

class CostSolver {
  public:
    static const UINT32 MAX_COST_SITE = 2;
    enum Site {
        CPU = 0,
        PIM = 1
    };
    static const std::string SiteName[MAX_COST_SITE];
    static const UINT32 MAX_EDGE_TYPE = 2;
    enum EdgeType {
        CONTROL, DATA
    };

  public:
    class CostTerm;
    /// A DECISION is a vector that represents a certain offloading decision, for example:
    /// A DECISION vector (PIM, CPU, CPU, PIM) means:
    /// put the 1st and 4th BBL on PIM and 2nd and 3rd on CPU for execution
    /// The target of CostSolver is to figure out the decision that will lead to the minimum total cost.
    typedef std::vector<Site> DECISION;

  private:
    static COST unitcontrolcost[MAX_COST_SITE][MAX_COST_SITE];

  private:
    static BBLID BBLSize;

    /// Based on the structure of the program, the total cost can be expressed as a polynomial, where each term is the product of some decisions,
    /// when the desision is given, the corresponding cost is determined, for example:
    /// Let "DECISION dec" be an arbitrary decision vector, the total cost may have an expression in this form:
    /// cost = coeff[0] + coeff[1] * dec[2] * dec[4] + coeff[2] * dec[0] * dec[1] * dec[2] * dec[3] + ...
    static std::vector<CostTerm> CostTermList;

  public:
    CostSolver();
    CostSolver(const std::string filename);
    static COST Cost(DECISION &decision);
    static VOID ReadConfig(const std::string filename);

    /// Read the Control Flow Graph from LLVM pass
    /// Attribute the control cost to the tail node
    static VOID AddControlCost(const std::string filename);

    static VOID AddCostToBBL();

};

class CostSolver::CostTerm {
    friend class CostSolver;
  private:
    COST coefficient;
    std::vector<BBLID> varproduct;
    
  public:
    COST Cost(DECISION &decision);
};


class PinInstrument {

  private:
    static MemoryLatency memory_latency;
    static InstructionLatency instruction_latency;
    static std::stack<BBLID> bblidstack;
    static CostSolver solver;

  public:
    static const BBLID GLOBALBBLID = -1;
  public:
    PinInstrument() {};

  public:
    static VOID DoAtAnnotatorHead(BBLID bblid);
    static VOID DoAtAnnotatorTail(BBLID bblid);

    static inline BBLID GetCurrentBBL()
    {
        return bblidstack.top();
    }

    /// The instrumentation function for an entire image
    static VOID Image(IMG img, VOID *v);

    /// Finalization
    static VOID Fini(INT32 code, VOID *v);
};


} // namespace PIMProf


#endif // __PININSTRUMENT_H__