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
#include "pin.H"

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


class CostGraph {
  public:
    typedef UINT32 COST;
    static const UINT32 MAX_COST_SITE = 2;
    enum {
        PIM, CPU
    };

  public:
    class Node;
    class Edge;

  public:
    std::vector<Node> nodelist;
    std::vector<Edge> edgelist;

  public:
    CostGraph(const std::string filename);
};

class CostGraph::Node {
  private:
    COST cost[MAX_COST_SITE];
    std::vector<Edge> adjacency;
};

class CostGraph::Edge {
  private:
    Node head;
    Node tail;
    /// e.g., cost[PIM][CPU] represents the edge cost if the head resides on PIM and tail resides on CPU
    COST cost[MAX_COST_SITE][MAX_COST_SITE];
    
};

class PinInstrument {
  public:
    typedef UINT32 BBLID;

  private:
    static MemoryLatency memory_latency;
    static InstructionLatency instruction_latency;
    static std::stack<BBLID> bblidstack;
    static CostGraph graph;

  public:
    PinInstrument();

  public:
    

    static VOID DoAtAnnotatorHead(BBLID bblid);
    static VOID DoAtAnnotatorTail(BBLID bblid);

    inline UINT32 GetCurrentBBL()
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