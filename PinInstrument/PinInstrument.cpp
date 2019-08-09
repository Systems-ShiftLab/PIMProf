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
CostSolver PinInstrument::solver;

TrieNode* DataReuse::_root;

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
double CostSolver::_data_reuse_threshold;
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

VOID DataReuse::UpdateTrie(DataReuseSegment &seg)
{
    // A reuse chain segment of size 1 can be removed
    if (seg.size() <= 1) return;

    // seg.print(std::cout);

    TrieNode *curNode = _root;
    std::set<BBLID>::iterator it = seg._set.begin();
    std::set<BBLID>::iterator eit = seg._set.end();
    for (; it != eit; it++) {
        BBLID curID = *it;
        TrieNode *temp = curNode->_children[curID];
        if (temp == NULL) {
            temp = new TrieNode();
            curNode->_children[curID] = temp;
        }
        curNode = temp;
    }
    TrieNode *temp = curNode->_children[seg._headID];
    if (temp == NULL) {
        temp = new TrieNode();
        curNode->_children[seg._headID] = temp;
    }
    temp->_isLeaf = true;
    temp->_count += 1;
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

VOID DataReuse::PrintTrie(std::ostream &out, BBLID bblid, TrieNode *root, int parent, int &count)
{
    if (root->_isLeaf) {
        out << "    V_" << count << " [shape=box, label=\"head = " << bblid << "\n cnt = " << root->_count << "\"];" << std::endl;
        out << "    V_" << parent << " -> V_" << count << ";" << std::endl;
        parent = count;
        count++;
    }
    else {
        out << "    V_" << count << " [label=\"" << bblid << "\"];" << std::endl;
        out << "    V_" << parent << " -> V_" << count << ";" << std::endl;
        parent = count;
        count++;
        std::map<BBLID, TrieNode *>::iterator it = root->_children.begin();
        std::map<BBLID, TrieNode *>::iterator eit = root->_children.end();
        for (; it != eit; it++) {
            DataReuse::PrintTrie(out, it->first, it->second, parent, count);
        }
    }
}

std::ostream &DataReuse::print(std::ostream &out) {
    int parent = 0;
    int count = 1;
    out << "digraph trie {" << std::endl;
    std::map<BBLID, TrieNode *>::iterator it = _root->_children.begin();
    std::map<BBLID, TrieNode *>::iterator eit = _root->_children.end();
    out << "    V_0" << " [label=\"root\"];" << std::endl;
    for (; it != eit; it++) {
        DataReuse::PrintTrie(out, it->first, it->second, parent, count);
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
    _data_reuse_threshold = 0;
}

CostSolver::CostSolver(const std::string filename)
{
    CostSolver();
    ReadControlFlowGraph(filename);
}

bool comp(const std::pair<COST, UINT32> &l, const std::pair<COST, UINT32> &r)
{
    return l.first < r.first;
}

CostSolver::DECISION CostSolver::Minimize(std::ostream &out)
{
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < CostSolver::_BBL_size; j++) {
            _BBL_partial_total[i][j]
                = CostSolver::_BBL_instruction_cost[i][j] * _instruction_multiplier[i]
                + CostSolver::_BBL_memory_cost[i][j];
        }
    }
    std::vector<std::pair<COST, UINT32>> index;
    DECISION decision, result;
    COST cur_partial_total = 0;
    

    for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
        COST diff = _BBL_partial_total[CPU][i] - _BBL_partial_total[PIM][i];
        index.push_back(std::make_pair(std::abs(diff), i));
    }

    for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
        if (_BBL_partial_total[CPU][i] <= _BBL_partial_total[PIM][i]) {
            decision.push_back(CPU);
            cur_partial_total += _BBL_partial_total[CPU][i];
        }
        else {
            decision.push_back(PIM);
            cur_partial_total += _BBL_partial_total[PIM][i];
        }
    }
    
    COST cur_total = Cost(decision);

    // PrintDecision(std::cout, decision, true);
    std::ofstream tempofs("greedy_decision.txt", std::ofstream::out);
    PrintDecision(tempofs, decision, false);

    std::cout << "instrcnt: " << mem_instr_cnt << " & " << nonmem_instr_cnt << " / " << instr_cnt << std::endl;
    std::cout << "PLAN\t\tINSTRUCTION\tMEMORY\t\tPARTIAL\t\tREUSE\t\tTOTAL" << std::endl;
    PrintDecisionStat(std::cout, decision, "Pure greedy");


    std::sort(index.begin(), index.end(), comp);

    decision.clear();
    cur_partial_total = 0;
    for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
        decision.push_back(CPU);
    }

    for (int j = 0; j < 10; j++) {
        for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
            BBLID id = index[i].second;
            
            if (decision[id] == CPU) {
                decision[id] = PIM;
                COST temp_total = Cost(decision);
                if (temp_total > cur_total)
                    decision[id] = CPU;
                else
                    cur_total = temp_total;
            }
            else {
                decision[id] = CPU;
                COST temp_total = Cost(decision);
                if (temp_total > cur_total)
                    decision[id] = PIM;
                else {
                    cur_total = temp_total;
                }
            }
        }
        std::cout << cur_total << std::endl;
    }
    // PrintDecision(std::cout, decision, true);
    PrintDecision(out, decision, false);
    PrintDecisionStat(std::cout, decision, "PIMProf opt");
    result = decision;

    // figure out the cost of pure CPU
    decision.clear();
    cur_partial_total = 0;
    for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
        decision.push_back(CPU);
    }
    
    PrintDecisionStat(std::cout, decision, "Pure CPU");


     // figure out the cost of pure PIM
    decision.clear();
    cur_partial_total = 0;
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

COST CostSolver::Cost(const CostSolver::DECISION &decision)
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
        if (decision[i] == CPU) {
            cur_instr_cost += CostSolver::_BBL_instruction_cost[CPU][i] * _instruction_multiplier[CPU];
            cur_mem_cost += CostSolver::_BBL_memory_cost[CPU][i];
        }
        else {
            cur_instr_cost += CostSolver::_BBL_instruction_cost[PIM][i] * _instruction_multiplier[PIM];
            cur_mem_cost += CostSolver::_BBL_memory_cost[PIM][i];
        }
    }
    return (cur_reuse_cost + cur_instr_cost + cur_mem_cost);
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
    if (cost >= 0) {
        _clwb_cost = cost;
    }
    cost = reader.GetReal("UnitReuseCost", "invalidate", -1);
    if (cost >= 0) {
        _invalidate_cost = cost;
    }
    cost = reader.GetReal("UnitReuseCost", "fetch", -1);
    if (cost >= 0) {
        _fetch_cost = cost;
    }
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        COST cost = reader.GetReal("UnitInstructionCost", CostSiteName[i], -1);
        if (cost >= 0) {
            _instruction_multiplier[i] = cost;
        }
    }
    double th = reader.GetReal("Threshold", "DataReuse", -1);
    if (th >= 0) {
        _data_reuse_threshold = th;
    }
}

VOID CostSolver::SetBBLSize(BBLID _BBL_size) {
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        CostSolver::_BBL_partial_total[i].resize(_BBL_size);
        memset(&CostSolver::_BBL_partial_total[i][0], 0, _BBL_size * sizeof CostSolver::_BBL_partial_total[i][0]);
    }
}

// VOID CostSolver::AddCostTerm(const CostTerm &cost) {
//     if (cost._coefficient == 0) {
//         return;
//     }
//     std::set<CostTerm>::iterator it = _cost_term_set.find(cost);
//     if (it != _cost_term_set.end()) {
//         it->_coefficient += cost._coefficient;
//     }
//     else {
//         _cost_term_set.insert(cost);
//     }
// }

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

    // /****************************
    // The control cost of BBL i -> BBL j depends on the offloading decision of BBL i and BBL j
    // totalcost += cc[0][0]*(1-d[i])*(1-d[j]) + cc[0][1]*(1-d[i])*d[j] + cc[1][0]*d[i]*(1-dec[j]) + cc[1][1]*d[i]*d[j]
    // ****************************/

    // while(getline(ifs, curline)) {
    //     std::stringstream ss(curline);
    //     BBLID head, tail;
    //     ss >> head;
    //     while (ss >> tail) {
    //         std::vector<INT32> list;
    //         list.push_back(-head);
    //         list.push_back(-tail);
    //         AddCostTerm(CostTerm(_control_latency[0][0], list));
    //         list.clear();
    //         list.push_back(-head);
    //         list.push_back(tail);
    //         AddCostTerm(CostTerm(_control_latency[0][1], list));
    //         list.clear();
    //         list.push_back(head);
    //         list.push_back(-tail);
    //         AddCostTerm(CostTerm(_control_latency[1][0], list));
    //         list.clear();
    //         list.push_back(head);
    //         list.push_back(tail);
    //         AddCostTerm(CostTerm(_control_latency[1][1], list));
    //     }
    // }
}

// VOID CostSolver::AddInstructionCost(std::vector<COST> (&_BBL_instruction_cost)[MAX_COST_SITE])
// {
//     for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
//         ASSERTX(_BBL_instruction_cost[i].size() == _BBL_size);
//     }

//     /****************************
//     The instruction cost of BBL i depends solely on the offloading decision of BBL i
//     totalcost += cc[0]*multiplier[0]*(1-d[i]) + cc[1]*multiplier[1]*d[i] or
//     totalcost += (-cc[0]*multiplier[0]+cc[1]*multiplier[1])*d[i] + cc[0]*multiplier[0]
//     ****************************/
//     for (BBLID i = 0; i < _BBL_size; i++) {
//         COST cost = -_BBL_instruction_cost[0][i] * _instruction_multiplier[0] + _BBL_instruction_cost[1][i] * _instruction_multiplier[1];
//         AddCostTerm(CostTerm(cost, i));
//         cost = _BBL_instruction_cost[0][i] * _instruction_multiplier[0];
//         AddCostTerm(CostTerm(cost));
//     }
// }

// VOID CostSolver::AddDataReuseCost(std::vector<BBLOP> *op)
// {
//     std::vector<BBLOP>::iterator it = op->begin();
//     std::vector<BBLOP>::iterator eit = op->end();
//     std::vector<INT32> origin;
//     std::vector<INT32> flipped;
//     std::ofstream ofs("output.txt", std::ofstream::app);
//     bool printflag = false;
//     for (; it != eit; it++) {
//         ofs << it->first << "," << it->second << " -> ";
//         printflag = true;
//         BBLID curid = it->first;
//         ACCESS_TYPE curtype = it->second;

//         /****************************
//         The data reuse cost of a LOAD is dependent on the offloading decision of all operations between itself and the most recent STORE. 
//         A PIM LOAD needs to pay the FLUSH cost when the LOADs and the most recent STORE are all on CPU.
//         A CPU LOAD needs to pay the FETCH cost when the LOADs and the most recent STORE are all on PIM.

//         Let the most recent STORE be on BBL i, and the LOAD itself be on BBL j, then:
//         totalcost += clwb*d[j]*(1-d[i])*(1-d[i+1])*...*(1-d[j-1])
//                    + fetch*(1-d[j])*d[i]*d[i+1]*...*d[j-1]

//         If there is no STORE before it,
//         A PIM LOAD does not need to pay any cost.
//         A CPU LOAD needs to pay the FETCH cost.

//         So:
//         totalcost += fetch*(1-d[j])
//         ****************************/
//         if (curtype == ACCESS_TYPE_LOAD) {
//             if (origin.empty()) { // no STORE
//                 AddCostTerm(CostTerm(_fetch_cost, -curid));
//                 origin.push_back(curid);
//                 flipped.push_back(-curid);
//             }
//             else {
//                 flipped.push_back(curid);
//                 AddCostTerm(CostTerm(_clwb_cost, flipped));
//                 flipped.pop_back();
//                 flipped.push_back(-curid);

//                 origin.push_back(-curid);
//                 AddCostTerm(CostTerm(_fetch_cost, origin));
//                 origin.pop_back();
//                 origin.push_back(curid);
//             }
//         }
//         /****************************
//         The data reuse cost of a STORE is dependent on the offloading decision of all operations between itself and the most recent STORE besides itself. 
//         A PIM STORE needs to pay the INVALIDATE cost when any one among the LOADs and the most recent STORE is on CPU.
//         A CPU STORE needs to pay the FETCH cost when the LOADs and the most recent STORE are all on PIM.

//         Let the most recent STORE be on BBL i, and the LOAD itself be on BBL j, then:
//         totalcost += invalidate*d[j]*(1-d[i]*d[i+1]*...*d[j-1])
//                    + fetch*(1-d[j])*d[i]*d[i+1]*...*d[j-1]

//         If there is no STORE before it,
//         A PIM STORE does not need to pay any cost.
//         A CPU STORE needs to pay the FETCH cost.

//         So:
//         totalcost += fetch*(1-d[j])
//         ****************************/
//         if (curtype == ACCESS_TYPE_STORE) {
//             if (origin.empty()) { // no STORE
//                 AddCostTerm(CostTerm(_fetch_cost, -curid));
//                 origin.push_back(curid);
//                 flipped.push_back(-curid);
//             }
//             else {
//                 AddCostTerm(CostTerm(_invalidate_cost, curid));
//                 origin.push_back(curid);
//                 AddCostTerm(CostTerm(-_invalidate_cost, origin));
//                 origin.pop_back();

//                 origin.push_back(-curid);
//                 AddCostTerm(CostTerm(_fetch_cost, origin));

//                 origin.clear();
//                 origin.push_back(curid);
//                 flipped.clear();
//                 flipped.push_back(-curid);
//             }
//         }
//         if (curtype == ACCESS_TYPE_NUM) {
//             ASSERTX(0);
//         }

//     }
//     if (printflag)
//         ofs << std::endl;
// }

std::ostream &CostSolver::PrintDecision(std::ostream &out, const DECISION &decision, bool toscreen)
{
    if (toscreen == true) {
        for (UINT32 i = 0; i < _BBL_size; i++) {
            out << i << ":";
            if (decision[i] == CPU) {
                out << "C";
            }
            else {
                out << "P";
            }
            out << " ";
        }
        out << std::endl;
    }
    else {
        for (UINT32 i = 0; i < _BBL_size; i++) {
            out << i << " ";
            if (decision[i] == CPU) {
                out << "C";
            }
            else {
                out << "P";
            }
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
        if (decision[i] == CPU) {
            cur_instr_cost += CostSolver::_BBL_instruction_cost[CPU][i] * _instruction_multiplier[CPU];
            cur_mem_cost += CostSolver::_BBL_memory_cost[CPU][i];
        }
        else {
            cur_instr_cost += CostSolver::_BBL_instruction_cost[PIM][i] * _instruction_multiplier[PIM];
            cur_mem_cost += CostSolver::_BBL_memory_cost[PIM][i];
        }
    }

    out << name << ":\t"
        << cur_instr_cost << "\t" << cur_mem_cost << "\t"
        << (cur_instr_cost + cur_mem_cost) << "\t" << cur_reuse_cost << "\t"
        << (cur_instr_cost + cur_mem_cost + cur_reuse_cost) << std::endl;
    return out;
}

/* ===================================================================== */
/* CostSolver::CostTerm */
/* ===================================================================== */

// CostSolver::CostTerm::CostTerm(COST c, INT32 id) : _coefficient(c)
// {
//     AddVar(id);
// }

// CostSolver::CostTerm::CostTerm(COST c, std::vector<INT32> &v) : _coefficient(c)
// {
//     AddVar(v);
// }

// COST CostSolver::CostTerm::Cost(DECISION &decision) const
// {
//     std::set<INT32>::const_iterator it = _varproduct.begin();
//     std::set<INT32>::const_iterator eit = _varproduct.end();
//     BOOL result = true;
//     for (; it != eit; it++) {
//         if (*it > 0)
//             result &= decision[*it];
//         else
//             result &= (!decision[-(*it)]);
//     }
//     return (_coefficient * result);
// }

// VOID CostSolver::CostTerm::AddVar(INT32 id)
// {
//     if (_varproduct.find(-id) != _varproduct.end())
//         _coefficient = 0;
//     if (_coefficient != 0)
//         _varproduct.insert(id);
// }

// VOID CostSolver::CostTerm::AddVar(std::vector<INT32> &v)
// {
//     std::vector<INT32>::iterator vit = v.begin();
//     std::vector<INT32>::iterator evit = v.end();
//     for (; vit != evit; vit++) {
//         if (_varproduct.find(-(*vit)) != _varproduct.end())
//             _coefficient = 0;
//         if (_coefficient != 0)
//             _varproduct.insert(*vit);
//     }
// }

// std::ostream &CostSolver::CostTerm::print(std::ostream &out) const
// {
//     std::set<INT32>::const_iterator it = _varproduct.begin();
//     std::set<INT32>::const_iterator eit = _varproduct.end();
//     if (_coefficient < 0)
//         out << "(" << _coefficient << ")";
//     else if (_coefficient > 0)
//         out << _coefficient;
//     else
//         return out;

//     if (_varproduct.size() == 0) return out;
//     if (*it > 0)
//         out << " * d[" << *it << "]";
//     else
//         out << " * (1 - d[" << -(*it) << "])";
//     if (_varproduct.size() == 1) return out;
//     for (it++; it != eit; it++) {
//         if (*it > 0)
//         out << " * d[" << *it << "]";
//         else 
//             out << " * (1 - d[" << -(*it) << "])";
//     }
//     return out;
// }


/* ===================================================================== */
/* PinInstrument */
/* ===================================================================== */

VOID PinInstrument::DoAtAnnotatorHead(BBLID bblid)
{
    // std::cout << std::dec << "PIMProfHead: " << bblid << std::endl;
    bblidstack.push(bblid);
}

VOID PinInstrument::DoAtAnnotatorTail(BBLID bblid)
{
    // std::cout << std::dec << "PIMProfTail: " << bblid << std::endl;
    ASSERTX(bblidstack.top() == bblid);
    bblidstack.pop();
}

VOID PinInstrument::ImageInstrument(IMG img, VOID *v)
{
    // push a fake bblid

    bblidstack.push(GLOBALBBLID);

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
            IARG_END);
        RTN_Close(annotator_head);

        RTN_Open(annotator_tail);
        RTN_InsertCall(
            annotator_tail,
            IPOINT_BEFORE,
            (AFUNPTR)DoAtAnnotatorTail,
            IARG_FUNCARG_CALLSITE_VALUE, 0, // The first argument of DoAtAnnotatorTail
            IARG_END);
        RTN_Close(annotator_tail);
    }
}

VOID PinInstrument::FinishInstrument(INT32 code, VOID *v)
{
    // CostSolver::AddInstructionCost(CostSolver::_BBL_instruction_cost);
    // CostSolver::print(std::cout);
    // std::cout << std::endl;
    // InstructionLatency::WriteConfig("template.ini");
    // printf("wow\n");
    char *outputfile = (char *)v;
    std::ofstream ofs(outputfile, std::ofstream::out);
    delete outputfile;
    CostSolver::DECISION decision = CostSolver::Minimize(ofs);
    ofs.close();
    
    ofs.open("BBLBlockCost.out", std::ofstream::out);
    ofs << "BBL\t" << "Decision\t"
    << "CPUIns\t\t" << "PIMIns\t\t"
    << "CPUMem\t\t" << "PIMMem\t\t"
    << "difference" << std::endl;
    for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
        ofs << i << "\t"
        << (decision[i] == 0 ? "C" : "P") << "\t"
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
    DataReuse::print(ofs);
    ofs.close();
}
