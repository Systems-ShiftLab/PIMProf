//===- PinInstrument.cpp - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <unistd.h>
#include <cmath>


#include "../LLVMAnalysis/Common.h"
#include "INIReader.h"
#include "PinInstrument.h"
#include "Cache.h"

using namespace PIMProf;

/* ===================================================================== */
/* Static data structure */
/* ===================================================================== */

MemoryLatency PinInstrument::memory_latency;
InstructionLatency PinInstrument::instruction_latency;
DataReuse PinInstrument::data_reuse;
std::stack<BBLID> PinInstrument::bblidstack;
bool inOpenMPRegion;
CostSolver PinInstrument::solver;

TrieNode *DataReuse::_root;
std::vector<TrieNode *> DataReuse::_leaves;

CACHE MemoryLatency::cache;

COST InstructionLatency::_instruction_latency[MAX_COST_SITE][MAX_INDEX];

COST CostSolver::_control_latency[MAX_COST_SITE][MAX_COST_SITE];
std::vector<COST> CostSolver::_BBL_instruction_cost[MAX_COST_SITE];
std::vector<COST> CostSolver::_BBL_memory_cost[MAX_COST_SITE];
std::vector<COST> CostSolver::_BBL_partial_total[MAX_COST_SITE];
COST CostSolver::_instruction_multiplier[MAX_COST_SITE];
COST CostSolver::_clwb_cost;
COST CostSolver::_invalidate_cost;
COST CostSolver::_fetch_cost;
COST CostSolver::_memory_cost[MAX_COST_SITE];
BBLID CostSolver::_BBL_size;
int CostSolver::_batchcount;
int CostSolver::_batchsize;
// std::set<CostSolver::CostTerm> CostSolver::_cost_term_set;
long long int instr_cnt = 0, mem_instr_cnt = 0, nonmem_instr_cnt = 0;

/* ===================================================================== */
/* DataReuse */
/* ===================================================================== */

DataReuse::DataReuse()
{
    _root = new TrieNode();
}

DataReuse::~DataReuse()
{
    DeleteTrie(_root);
}

VOID DataReuse::UpdateTrie(TrieNode *root, DataReuseSegment &seg)
{
    // A reuse chain segment of size 1 can be removed
    if (seg.size() <= 1) return;

    // seg.print(std::cout);

    TrieNode *curNode = root;
    std::set<BBLID>::iterator it = seg._set.begin();
    std::set<BBLID>::iterator eit = seg._set.end();
    for (; it != eit; it++) {
        BBLID curID = *it;
        TrieNode *temp = curNode->_children[curID];
        if (temp == NULL) {
            temp = new TrieNode();
            temp->_parent = curNode;
            temp->_curID = curID;
            curNode->_children[curID] = temp;
        }
        curNode = temp;
    }
    TrieNode *temp = curNode->_children[seg._headID];
    if (temp == NULL) {
        temp = new TrieNode();
        temp->_parent = curNode;
        temp->_curID = seg._headID;
        curNode->_children[seg._headID] = temp;
        _leaves.push_back(temp);
    }
    temp->_isLeaf = true;
    temp->_count += seg.getCount();
}

VOID DataReuse::ExportSegment(DataReuseSegment &seg, TrieNode *leaf)
{
    ASSERTX(leaf->_isLeaf);
    seg.setHead(leaf->_curID);
    seg.setCount(leaf->_count);

    TrieNode *temp = leaf;
    while (temp->_parent != NULL) {
        seg.insert(temp->_curID);
        temp = temp->_parent;
    }
}

VOID DataReuse::DeleteTrie(TrieNode *root)
{
    if (!root->_isLeaf) {
        std::map<BBLID, TrieNode *>::iterator it = root->_children.begin();
        std::map<BBLID, TrieNode *>::iterator eit = root->_children.end();
        for (; it != eit; it++) {
            DataReuse::DeleteTrie(it->second);
        }
    }
    delete root;
}

VOID DataReuse::PrintTrie(std::ostream &out, TrieNode *root, int parent, int &count)
{
    if (root->_isLeaf) {
        out << "    V_" << count << " [shape=box, label=\"head = " << root->_curID << "\n cnt = " << root->_count << "\"];" << std::endl;
        out << "    V_" << parent << " -> V_" << count << ";" << std::endl;
        parent = count;
        count++;
    }
    else {
        out << "    V_" << count << " [label=\"" << root->_curID << "\"];" << std::endl;
        out << "    V_" << parent << " -> V_" << count << ";" << std::endl;
        parent = count;
        count++;
        std::map<BBLID, TrieNode *>::iterator it = root->_children.begin();
        std::map<BBLID, TrieNode *>::iterator eit = root->_children.end();
        for (; it != eit; it++) {
            DataReuse::PrintTrie(out, it->second, parent, count);
        }
    }
}

std::ostream &DataReuse::print(std::ostream &out, TrieNode *root) {
    int parent = 0;
    int count = 1;
    out << "digraph trie {" << std::endl;
    std::map<BBLID, TrieNode *>::iterator it = root->_children.begin();
    std::map<BBLID, TrieNode *>::iterator eit = root->_children.end();
    out << "    V_0" << " [label=\"root\"];" << std::endl;
    for (; it != eit; it++) {
        DataReuse::PrintTrie(out, it->second, parent, count);
    }
    out << "}" << std::endl;
    return out;
}

/* ===================================================================== */
/* MemoryLatency */
/* ===================================================================== */

VOID MemoryLatency::InstrCacheRef(ADDRINT addr)
{
    cache.InstrCacheRef(addr);
}

VOID MemoryLatency::DataCacheRefMulti(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    cache.DataCacheRefMulti(addr, size, accessType);
}

VOID MemoryLatency::DataCacheRefSingle(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    cache.DataCacheRefSingle(addr, size, accessType);
}

VOID MemoryLatency::InstructionInstrument(INS ins, VOID *v)
{
    // all instruction fetches access I-cache
    INS_InsertCall(
        ins, IPOINT_BEFORE, (AFUNPTR)InstrCacheRef,
        IARG_INST_PTR,
        IARG_END);
    if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins))
    {
        const UINT32 size = INS_MemoryReadSize(ins);
        const AFUNPTR countFun = (size <= 4 ? (AFUNPTR)DataCacheRefSingle : (AFUNPTR)DataCacheRefMulti);

        // only predicated-on memory instructions access D-cache
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, countFun,
            IARG_MEMORYREAD_EA,
            IARG_MEMORYREAD_SIZE,
            IARG_UINT32, ACCESS_TYPE_LOAD,
            IARG_END);
    }
    if (INS_IsMemoryWrite(ins) && INS_IsStandardMemop(ins))
    {
        const UINT32 size = INS_MemoryWriteSize(ins);
        const AFUNPTR countFun = (size <= 4 ? (AFUNPTR)DataCacheRefSingle : (AFUNPTR)DataCacheRefMulti);

        // only predicated-on memory instructions access D-cache
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, countFun,
            IARG_MEMORYWRITE_EA,
            IARG_MEMORYWRITE_SIZE,
            IARG_UINT32, ACCESS_TYPE_STORE,
            IARG_END);
    }
}

VOID MemoryLatency::FinishInstrument(INT32 code, VOID *v)
{
    cache.WriteStats("stats.out");
}

VOID MemoryLatency::ReadConfig(const std::string filename)
{
    cache.ReadConfig(filename);
    
    INIReader reader(filename);
    ASSERTX(!INIErrorMsg(reader.ParseError(), filename, std::cerr));
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        COST cost = reader.GetReal("Memory", CostSiteName[i] + "memorycost", -1);
        if (cost >= 0) {
            CostSolver::_memory_cost[i] = cost;
        }
    }
}

std::ostream& MemoryLatency::WriteConfig(std::ostream& out)
{
    // for (UINT32 i = 0; i < MAX_LEVEL; i++) {
    //     out << "[" << _name[i] << "]" << std::endl;
    //         << "linesize = " << _cache[i]->
    // }
    out << "Not implemented" << std::endl;
    return out;
}

VOID MemoryLatency::WriteConfig(const std::string filename)
{
    ofstream out;
    out.open(filename.c_str(), ios_base::out);
    WriteConfig(out);
    out.close();
}

/* ===================================================================== */
/* InstructionLatency */
/* ===================================================================== */


InstructionLatency::InstructionLatency()
{
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < MAX_INDEX; j++) {
            _instruction_latency[i][j] = 1;
        }
    }
}

InstructionLatency::InstructionLatency(const std::string filename)
{
    InstructionLatency();
    ReadConfig(filename);
}

VOID InstructionLatency::SetBBLSize(BBLID _BBL_size) {
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        CostSolver::_BBL_instruction_cost[i].resize(_BBL_size);
        memset(&CostSolver::_BBL_instruction_cost[i][0], 0, _BBL_size * sizeof CostSolver::_BBL_instruction_cost[i][0]);
    }
}

VOID MemoryLatency::SetBBLSize(BBLID _BBL_size) {
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        CostSolver::_BBL_memory_cost[i].resize(_BBL_size);
        memset(&CostSolver::_BBL_memory_cost[i][0], 0, _BBL_size * sizeof CostSolver::_BBL_memory_cost[i][0]);
    }
}

VOID InstructionLatency::InstructionCount(UINT32 opcode, BOOL ismem)
{
    instr_cnt++;
    BBLID bblid = PinInstrument::GetCurrentBBL();
    if (bblid == GLOBALBBLID) return;


    if (ismem) {
        mem_instr_cnt++;
    }
    else {
        nonmem_instr_cnt++;
        for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
            CostSolver::_BBL_instruction_cost[i][bblid] += _instruction_latency[i][opcode];
        }
    }
}

VOID InstructionLatency::InstructionInstrument(INS ins, VOID *v)
{
    UINT32 opcode = (UINT32)(INS_Opcode(ins));
    BOOL ismem = INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)InstructionCount,
        IARG_ADDRINT, opcode,
        IARG_BOOL, ismem,
        IARG_END);
}


VOID InstructionLatency::ReadConfig(const std::string filename)
{
    INIReader reader(filename);
    ASSERTX(!INIErrorMsg(reader.ParseError(), filename, std::cerr));
    
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < MAX_INDEX; j++) {
            std::string opcodestr = OPCODE_StringShort(j);
            if (opcodestr != "LAST") {
                COST latency = reader.GetReal(CostSiteName[i] + "InstructionLatency", opcodestr, -1);
                if (latency >= 0) {
                    _instruction_latency[i][j] = latency;
                }
            }
        }
    }
}

std::ostream& InstructionLatency::WriteConfig(std::ostream& out)
{
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        out << ("[" + CostSiteName[i] + "InstructionLatency]") << std::endl
            << "; <Instuction Name> = <Instruction Latency>" << std::endl;
        for (UINT32 j = 0; j < MAX_INDEX; j++)
        {
            std::string opcodestr = OPCODE_StringShort(j);
            if (opcodestr != "LAST") {
                opcodestr = ljstr(opcodestr, 15);
                out << opcodestr << "= " << _instruction_latency[i][j] << std::endl;
            }
        }
        out << std::endl;
    }
    return out;
}

VOID InstructionLatency::WriteConfig(const std::string filename)
{
    std::ofstream out;
    out.open(filename.c_str(), ios_base::out);
    WriteConfig(out);
    out.close();
}

/* ===================================================================== */
/* CostSolver */
/* ===================================================================== */
CostSolver::CostSolver()
{
    memset(_control_latency, 0, sizeof(_control_latency));
    
    _BBL_size = 0;

    _instruction_multiplier[PIM] = 1;
    _instruction_multiplier[CPU] = 1;
    _clwb_cost = 0;
    _invalidate_cost = 0;
    _fetch_cost = 0;
    _batchcount = 0;
    _batchsize = 0;
}

CostSolver::CostSolver(const std::string filename)
{
    CostSolver();
    ReadControlFlowGraph(filename);
}


CostSolver::DECISION CostSolver::PrintSolution(std::ostream &out)
{
    // set partial total
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < CostSolver::_BBL_size; j++) {
            _BBL_partial_total[i][j]
                = CostSolver::_BBL_instruction_cost[i][j] * _instruction_multiplier[i]
                + CostSolver::_BBL_memory_cost[i][j];
        }
    }

    DECISION decision, result;

    std::cout << "instrcnt: " << mem_instr_cnt << " & " << nonmem_instr_cnt << " / " << instr_cnt << std::endl;
    std::cout << "PLAN\t\tINSTRUCTION\tMEMORY\t\tPARTIAL\t\tREUSE\t\tTOTAL" << std::endl;

    // greedy decision
    for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
        if (_BBL_partial_total[CPU][i] <= _BBL_partial_total[PIM][i]) {
            decision.push_back(CPU);
        }
        else {
            decision.push_back(PIM);
        }
    }
    std::ofstream tempofs("greedy_decision.txt", std::ofstream::out);
    PrintDecision(tempofs, decision, false);
    PrintDecisionStat(std::cout, decision, "Pure greedy");

    // Optimal
    result = FindOptimal(DataReuse::_root);
    // PrintDecision(std::cout, decision, true);
    PrintDecision(out, result, false);
    PrintDecisionStat(std::cout, result, "PIMProf opt");

    // pure CPU
    decision.clear();
    for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
        decision.push_back(CPU);
    }
    PrintDecisionStat(std::cout, decision, "Pure CPU");

     // pure PIM
    decision.clear();
    for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
        decision.push_back(PIM);
    }
    PrintDecisionStat(std::cout, decision, "Pure PIM");

    return result;
}

VOID CostSolver::TrieBFS(COST &cost, const CostSolver::DECISION &decision, BBLID bblid, TrieNode *root, bool isDifferent)
{
    if (root->_isLeaf) {
        if (isDifferent) {
            if (decision[bblid] == CPU) {
                cost += root->_count * _clwb_cost;
            }
            else {
                cost += root->_count * _fetch_cost;
            }
        }
    }
    else {
        std::map<BBLID, TrieNode *>::iterator it = root->_children.begin();
        std::map<BBLID, TrieNode *>::iterator eit = root->_children.end();
        for (; it != eit; it++) {
            if (isDifferent) {
                TrieBFS(cost, decision, it->first, it->second, true);
            }
            else if (decision[bblid] != decision[it->first]) {
                TrieBFS(cost, decision, it->first, it->second, true);
            }
            else {
                TrieBFS(cost, decision, it->first, it->second, false);
            }
        }
        
    }
}

COST CostSolver::Cost(const CostSolver::DECISION &decision, TrieNode *reusetree)
{
    COST cur_reuse_cost = 0;
    COST cur_instr_cost = 0;
    COST cur_mem_cost = 0;
    std::map<BBLID, TrieNode *>::iterator it = reusetree->_children.begin();
    std::map<BBLID, TrieNode *>::iterator eit = reusetree->_children.end();
    for (; it != eit; it++) {
        TrieBFS(cur_reuse_cost, decision, it->first, it->second, false);
    }
    for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
        CostSite site = decision[i];
        if (site == CPU || site == PIM) {
            cur_instr_cost += CostSolver::_BBL_instruction_cost[site][i] * _instruction_multiplier[site];
            cur_mem_cost += CostSolver::_BBL_memory_cost[site][i];
        }
    }
    return (cur_reuse_cost + cur_instr_cost + cur_mem_cost);
}

bool CostSolverComparator(const TrieNode *l, const TrieNode *r)
{
    return l->_count > r->_count;
}

CostSolver::DECISION CostSolver::FindOptimal(TrieNode *reusetree)
{

    std::sort(DataReuse::_leaves.begin(), DataReuse::_leaves.end(), CostSolverComparator);

    COST cur_total = FLT_MAX;
    DECISION decision;

    //initialize all decision to INVALID
    for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
        decision.push_back(INVALID);
    }

    TrieNode *partial_root = new TrieNode();
    DataReuseSegment allidset;
    int currentnode = 0;
    int leavessize = DataReuse::_leaves.size();

    for (int i = 0; i < _batchcount; i++) {
        std::cout << "cnt" << i << std::endl;
        
        std::vector<BBLID> idvec;
        // insert segments until the number of different BBLs hit _batchsize
        while (currentnode < leavessize) {
            DataReuseSegment seg;
            DataReuse::ExportSegment(seg, DataReuse::_leaves[currentnode]);
            std::vector<BBLID> diff = seg.diff(allidset);
            // std::cout << idvec.size() << " " << diff.size() << std::endl;
            if (idvec.size() + diff.size() > (unsigned)_batchsize) break;
            allidset.insert(seg);
            idvec.insert(idvec.end(), diff.begin(), diff.end());
            DataReuse::UpdateTrie(partial_root, seg);
            currentnode++;
        }

        int idvecsize = idvec.size();
        for (int i = 0; i < idvecsize; i++) {
            std::cout << idvec[i] << " ";
        }
        std::cout << std::endl;

        // find optimal in this batch
        ASSERTX(idvecsize <= _batchsize);
        UINT64 permute = (1 << idvecsize) - 1;

        // should not compare the cost between batches, so reset cur_total
        cur_total = FLT_MAX;

        DECISION temp_decision = decision;
        for (; permute != (UINT64)(-1); permute--) {
            for (int j = 0; j < idvecsize; j++) {
                if ((permute >> j) & 1)
                    temp_decision[idvec[j]] = PIM;
                else
                    temp_decision[idvec[j]] = CPU;
            }
            COST temp_total = Cost(temp_decision, partial_root);
            if (temp_total < cur_total) {
                cur_total = temp_total;
                decision = temp_decision;
            }
            // PrintDecision(std::cout, decision, true);
        }
    }
    std::ofstream ofs("temp.dot", std::ofstream::out);
    DataReuse::print(ofs, partial_root);
    ofs.close();

    DataReuse::DeleteTrie(partial_root);

    for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
        if (decision[i] == INVALID) {
            if (_BBL_partial_total[CPU][i] <= _BBL_partial_total[PIM][i]) {
                decision[i] = CPU;
            }
            else {
                decision[i] = PIM;
            }
        }
    }
    // for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
    //     if (decision[i] == INVALID)
    //         decision[i] = PIM;
    // }

    cur_total = Cost(decision, DataReuse::_root);
    std::cout << cur_total << std::endl;
    for (int j = 0; j < 10; j++) {
        for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
            BBLID id = i;
            
            if (decision[id] == CPU) {
                decision[id] = PIM;
                COST temp_total = Cost(decision, DataReuse::_root);
                if (temp_total > cur_total)
                    decision[id] = CPU;
                else
                    cur_total = temp_total;
            }
            else {
                decision[id] = CPU;
                COST temp_total = Cost(decision, DataReuse::_root);
                if (temp_total > cur_total)
                    decision[id] = PIM;
                else {
                    cur_total = temp_total;
                }
            }
        }
        std::cout << cur_total << std::endl;
    }

    return decision;
}

VOID CostSolver::ReadConfig(const std::string filename)
{
    INIReader reader(filename);
    ASSERTX(!INIErrorMsg(reader.ParseError(), filename, std::cerr));
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < MAX_COST_SITE; j++) {
            std::string coststr = CostSiteName[i] + "to" + CostSiteName[j];
            COST cost = reader.GetReal("UnitControlCost", coststr, -1);
            if (cost >= 0) {
                _control_latency[i][j] = cost;
            }
        }
    }
    COST cost = reader.GetReal("UnitReuseCost", "clwb", -1);
    ASSERTX(cost >= 0);
    _clwb_cost = cost;

    cost = reader.GetReal("UnitReuseCost", "invalidate", -1);
    ASSERTX(cost >= 0);
    _invalidate_cost = cost;

    cost = reader.GetReal("UnitReuseCost", "fetch", -1);
    ASSERTX(cost >= 0);
    _fetch_cost = cost;

    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        COST cost = reader.GetReal("UnitInstructionCost", CostSiteName[i], -1);
        ASSERTX(cost >= 0);
        _instruction_multiplier[i] = cost;
    }
    int size = reader.GetInteger("DataReuse", "BatchCount", -1);
    ASSERTX(size >= 0);
    _batchcount = size;

    size = reader.GetInteger("DataReuse", "BatchSize", -1);
    ASSERTX(size > 0);
    _batchsize = size;

}

VOID CostSolver::SetBBLSize(BBLID _BBL_size) {
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        CostSolver::_BBL_partial_total[i].resize(_BBL_size);
        memset(&CostSolver::_BBL_partial_total[i][0], 0, _BBL_size * sizeof CostSolver::_BBL_partial_total[i][0]);
    }
}

VOID CostSolver::ReadControlFlowGraph(const std::string filename)
{
    std::ifstream ifs;
    ifs.open(filename.c_str());
    std::string curline;

    getline(ifs, curline);
    std::stringstream ss(curline);
    ss >> _BBL_size;
    _BBL_size++; // _BBL_size = Largest BBLID + 1

    InstructionLatency::SetBBLSize(_BBL_size);
    MemoryLatency::SetBBLSize(_BBL_size);
    CostSolver::SetBBLSize(_BBL_size);
}

std::ostream &CostSolver::PrintDecision(std::ostream &out, const DECISION &decision, bool toscreen)
{
    if (toscreen == true) {
        for (UINT32 i = 0; i < _BBL_size; i++) {
            out << i << ":";
            out << (decision[i] == CPU ? "C" : "");
            out << (decision[i] == PIM ? "P" : "");
            out << " ";
        }
        out << std::endl;
    }
    else {
        for (UINT32 i = 0; i < _BBL_size; i++) {
            out << i << " ";
            out << (decision[i] == CPU ? "C" : "");
            out << (decision[i] == PIM ? "P" : "");
            out << std::endl;
        }
    }
    return out;
}

std::ostream &CostSolver::PrintDecisionStat(std::ostream &out, const DECISION &decision, const std::string &name)
{
    COST cur_reuse_cost = 0;
    COST cur_instr_cost = 0;
    COST cur_mem_cost = 0;
    std::map<BBLID, TrieNode *>::iterator it = DataReuse::_root->_children.begin();
    std::map<BBLID, TrieNode *>::iterator eit = DataReuse::_root->_children.end();
    for (; it != eit; it++) {
        TrieBFS(cur_reuse_cost, decision, it->first, it->second, false);
    }
    for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
        CostSite site = decision[i];
        if (site == CPU || site == PIM) {
            cur_instr_cost += CostSolver::_BBL_instruction_cost[site][i] * _instruction_multiplier[site];
            cur_mem_cost += CostSolver::_BBL_memory_cost[site][i];
        }
    }

    out << name << ":\t"
        << cur_instr_cost << "\t" << cur_mem_cost << "\t"
        << (cur_instr_cost + cur_mem_cost) << "\t" << cur_reuse_cost << "\t"
        << (cur_instr_cost + cur_mem_cost + cur_reuse_cost) << std::endl;
    return out;
}


/* ===================================================================== */
/* PinInstrument */
/* ===================================================================== */

VOID PinInstrument::DoAtAnnotatorHead(BBLID bblid, INT32 isomp)
{
    // std::cout << std::dec << "PIMProfHead: " << bblid << std::endl;
    bblidstack.push(bblid);
}

VOID PinInstrument::DoAtAnnotatorTail(BBLID bblid, INT32 isomp)
{
    // std::cout << std::dec << "PIMProfTail: " << bblid << std::endl;
    ASSERTX(bblidstack.top() == bblid);
    bblidstack.pop();
    inOpenMPRegion = false;
}

VOID PinInstrument::ImageInstrument(IMG img, VOID *v)
{
    // push a fake bblid

    bblidstack.push(GLOBALBBLID);
    inOpenMPRegion = false;

    // find annotator head and tail by their names
    RTN annotator_head = RTN_FindByName(img, PIMProfAnnotatorHead.c_str());
    RTN annotator_tail = RTN_FindByName(img, PIMProfAnnotatorTail.c_str());

    if (RTN_Valid(annotator_head) && RTN_Valid(annotator_tail))
    {
        // Instrument malloc() to print the input argument value and the return value.
        RTN_Open(annotator_head);
        RTN_InsertCall(
            annotator_head,
            IPOINT_BEFORE,
            (AFUNPTR)DoAtAnnotatorHead,
            IARG_FUNCARG_CALLSITE_VALUE, 0, // The first argument of DoAtAnnotatorHead
            IARG_FUNCARG_CALLSITE_VALUE, 1, // The second argument of DoAtAnnotatorTail
            IARG_END);
        RTN_Close(annotator_head);

        RTN_Open(annotator_tail);
        RTN_InsertCall(
            annotator_tail,
            IPOINT_BEFORE,
            (AFUNPTR)DoAtAnnotatorTail,
            IARG_FUNCARG_CALLSITE_VALUE, 0, // The first argument of DoAtAnnotatorTail
            IARG_FUNCARG_CALLSITE_VALUE, 1, // The second argument of DoAtAnnotatorTail
            IARG_END);
        RTN_Close(annotator_tail);
    }
}

VOID PinInstrument::FinishInstrument(INT32 code, VOID *v)
{
    char *outputfile = (char *)v;
    std::ofstream ofs(outputfile, std::ofstream::out);
    delete outputfile;
    CostSolver::DECISION decision = CostSolver::PrintSolution(ofs);
    ofs.close();
    
    ofs.open("BBLBlockCost.out", std::ofstream::out);
    ofs << "BBL\t" << "Decision\t"
    << "CPUIns\t\t" << "PIMIns\t\t"
    << "CPUMem\t\t" << "PIMMem\t\t"
    << "difference" << std::endl;
    for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
        ofs << i << "\t"
        << (decision[i] == CPU ? "C" : "") << (decision[i] == PIM ? "P" : "") << "\t"
        << CostSolver::_BBL_instruction_cost[CPU][i] *
           CostSolver::_instruction_multiplier[CPU]
        << "\t\t"
        << CostSolver::_BBL_instruction_cost[PIM][i] * 
           CostSolver::_instruction_multiplier[PIM]
        << "\t\t"
        << CostSolver::_BBL_memory_cost[CPU][i] << "\t\t"
        << CostSolver::_BBL_memory_cost[PIM][i] << "\t\t"
        << CostSolver::_BBL_partial_total[CPU][i] - CostSolver::_BBL_partial_total[PIM][i] << std::endl;
    }
    ofs.close();
    ofs.open("BBLReuseCost.dot", std::ofstream::out);
    DataReuse::print(ofs, DataReuse::getRoot());
    ofs.close();
}
