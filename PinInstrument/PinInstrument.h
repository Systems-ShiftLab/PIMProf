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
#include <set>
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
    static VOID MemRefMulti(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);

    /// Do on a single-line data cache reference
    static VOID MemRefSingle(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);

    /// The instrumentation function for memory instructions
    static VOID InstructionInstrument(INS ins, VOID *v);

    /// Finalization
    static VOID FinishInstrument(INT32 code, VOID * v);

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

  private:
    /// Construction of latency table follows the opcode generation function in
    /// $(PIN_ROOT)/source/tools/SimpleExamples/opcodemix.cpp
    static COST _instruction_latency[MAX_COST_SITE][MAX_INDEX];

  public:
    /// Default initialization.
    /// Initialize _instruction_latency with hard-coded instruction latency.
    InstructionLatency();

    /// Initialization with input config.
    InstructionLatency(const std::string filename);

    static VOID SetBBLSize(BBLID _BBL_size);

    /// The instrumentation function for normal instructions
    static VOID InstructionInstrument(INS ins, VOID *v);

  public:
    /// Read instruction latency config to _instruction_latency from ofstream or file.
    /// Invalid values (including negative latency, non-integer values) will be ignored.
    static VOID ReadConfig(const std::string filename);

    /// Write the current instruction latency config to ofstream or file.
    /// If no modification is made, then this will output the 
    /// default instruction latency config PIMProf will use.
    static std::ostream& WriteConfig(std::ostream& out);
    static VOID WriteConfig(const std::string filename);
};

class CostSolver {
    friend class InstructionLatency;
    friend class MemoryLatency;
    friend class PinInstrument;

  public:
    class CostTerm;
    /// A DECISION is a vector that represents a certain offloading decision, for example:
    /// A DECISION vector (PIM, CPU, CPU, PIM) means:
    /// put the 1st and 4th BBL on PIM and 2nd and 3rd on CPU for execution
    /// The target of CostSolver is to figure out the decision that will lead to the minimum total cost.
    typedef std::vector<CostSite> DECISION;

  private:
    static COST _control_latency[MAX_COST_SITE][MAX_COST_SITE];
    static std::vector<COST> _BBL_instruction_cost[MAX_COST_SITE];

  private:
    static BBLID _BBL_size;

    /// Based on the structure of the program, the total cost can be expressed as a polynomial, where each term is the product of some decisions,
    /// when the desision is given, the corresponding cost is determined, for example:
    /// Let "DECISION dec" be an arbitrary decision vector, the total cost may have an expression in this form:
    /// cost = coeff[0] + coeff[1] * dec[2] * dec[4] + coeff[2] * dec[0] * dec[1] * dec[2] * dec[3] + ...
    static std::set<CostTerm> _cost_term_set;

  public:
    CostSolver();
    CostSolver(const std::string filename);
    static inline VOID clear() { _cost_term_set.clear(); }
    static COST Cost(DECISION &decision);

    static VOID ReadConfig(const std::string filename);

    static VOID AddCostTerm(const CostTerm &cost);

    /// Read the Control Flow Graph from LLVM pass
    /// Attribute the control cost to the tail node
    static VOID AddControlCost(const std::string filename);

    /// Read the Instruction Cost from InstructionLatency instrumentation result
    static VOID AddInstructionCost(std::vector<COST> (&_BBL_instruction_cost)[MAX_COST_SITE]);

    static VOID AddDataReuseCost(std::vector<BBLOP> *op);

    static std::ostream &print(std::ostream &out);
};

class CostSolver::CostTerm {
    friend class CostSolver;
  public:
    typedef std::vector<BBLID> BBLIDList; 
  private:
    mutable COST _coefficient;
    std::set<BBLID> _varproduct;
    
  public:
    inline CostTerm(COST c = 0) : _coefficient(c) {}
    CostTerm(COST c, BBLID id);
    CostTerm(COST c, std::vector<BBLID> &v);

    COST Cost(DECISION &decision) const;

    inline VOID AddCoefficient(COST c) { _coefficient += c; }
    inline VOID AddVar(BBLID id);
    inline VOID AddVar(std::vector<BBLID> &v);

    /// use this comparator for _cost_term_set comparison
    bool operator < (const CostTerm &rhs) const 
    {
        return (_varproduct < rhs._varproduct);
    }

    std::ostream &print(std::ostream &out) const;
};


class PinInstrument {

  private:
    static MemoryLatency memory_latency;
    static InstructionLatency instruction_latency;
    static std::stack<BBLID> bblidstack;
    static CostSolver solver;

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
    static VOID ImageInstrument(IMG img, VOID *v);

    /// Finalization
    static VOID FinishInstrument(INT32 code, VOID *v);
};


} // namespace PIMProf


#endif // __PININSTRUMENT_H__