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

DataReuse::DataReuseSegment DataReuse::_seg;
DataReuse::TrieNode* DataReuse::_root;

CACHE MemoryLatency::cache;

COST InstructionLatency::_instruction_latency[MAX_COST_SITE][MAX_INDEX];

COST CostSolver::_control_latency[MAX_COST_SITE][MAX_COST_SITE];
std::vector<COST> CostSolver::_BBL_instruction_cost[MAX_COST_SITE];
std::vector<COST> CostSolver::_BBL_memory_cost[MAX_COST_SITE];
COST CostSolver::_instruction_multiplier[MAX_COST_SITE];
COST CostSolver::_clwb_cost;
COST CostSolver::_invalidate_cost;
COST CostSolver::_fetch_cost;
COST CostSolver::_memory_cost[MAX_COST_SITE];
BBLID CostSolver::_BBL_size;
std::set<CostSolver::CostTerm> CostSolver::_cost_term_set;

/* ===================================================================== */
/* DataReuse */
/* ===================================================================== */
size_t DataReuse::DataReuseSegment::size() {
    return _set.size();
}

VOID DataReuse::DataReuseSegment::insert(BBLID bblid) {
    if (_set.empty())
        _headID = bblid;
    _set.insert(bblid);
}

VOID DataReuse::DataReuseSegment::clear() {
    _set.clear();
}

VOID DataReuse::DataReuseSegment::setHead(BBLID head) {
    _headID = head;
}

BBLID DataReuse::DataReuseSegment::getHead() {
    return _headID;
}

BOOL DataReuse::DataReuseSegment::operator == (DataReuseSegment &rhs) {
    return (_headID == rhs._headID && _set == rhs._set);
}

std::ostream &print(std::ostream &out) {
    return out;
}

DataReuse::TrieNode::TrieNode()
{
    _isLeaf = false;
    _count = 0;
}

DataReuse::DataReuse()
{
    _root = new TrieNode();
}

DataReuse::~DataReuse()
{
    DeleteTrie(_root);
}

VOID DataReuse::UpdateTrie(DataReuse::DataReuseSegment &seg)
{
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
        out << "V_" << count << "[shape=box, label=\"" << bblid << "," << root->_count << "\"];" << std::endl;
        out << "V_" << parent << " -> V_" << count << ";" << std::endl;
        parent = count;
        count++;
    }
    else {
        out << "V_" << count << "[label=\"" << bblid << "," << root->_count << "\"];" << std::endl;
        out << "V_" << parent << " -> V_" << count << ";" << std::endl;
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
    int count = 0;
    int parent = 0;
    out << "digraph trie {" << std::endl;
    std::map<BBLID, TrieNode *>::iterator it = _root->_children.begin();
    std::map<BBLID, TrieNode *>::iterator eit = _root->_children.end();
    out << "V_" << count << " [label=\"root\"];" << std::endl;
    for (; it != eit; it++) {
        DataReuse::PrintTrie(out, it->first, it->second, parent, count);
    }
    out << "}" << std::endl;
    return out;
}

VOID DataReuse::Insert(BBLID bblid, ACCESS_TYPE accessType)
{
    _seg.insert(bblid);
    if (accessType == ACCESS_TYPE::ACCESS_TYPE_STORE) {
        DataReuse::UpdateTrie(_seg);
        _seg.clear();
        _seg.insert(bblid);
    }
}


VOID DataReuse::Split()
{
    DataReuse::UpdateTrie(_seg);
    _seg.clear();
}

/* ===================================================================== */
/* MemoryLatency */
/* ===================================================================== */

VOID MemoryLatency::InsRef(ADDRINT addr)
{
    cache.InsRef(addr);
}

VOID MemoryLatency::MemRefMulti(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    cache.MemRefMulti(addr, size, accessType);
}

VOID MemoryLatency::MemRefSingle(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    cache.MemRefSingle(addr, size, accessType);
}

VOID MemoryLatency::InstructionInstrument(INS ins, VOID *v)
{
    // all instruction fetches access I-cache
    INS_InsertCall(
        ins, IPOINT_BEFORE, (AFUNPTR)InsRef,
        IARG_INST_PTR,
        IARG_END);
    if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins))
    {
        const UINT32 size = INS_MemoryReadSize(ins);
        const AFUNPTR countFun = (size <= 4 ? (AFUNPTR)MemRefSingle : (AFUNPTR)MemRefMulti);

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
        const AFUNPTR countFun = (size <= 4 ? (AFUNPTR)MemRefSingle : (AFUNPTR)MemRefMulti);

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

VOID InstructionLatency::InstructionInstrument(INS ins, VOID *v)
{
    BBLID bblid = PinInstrument::GetCurrentBBL();
    if (bblid == GLOBALBBLID) return;
    // all instruction fetches access I-cache
    if (! (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)))
    {
        OPCODE opcode = INS_Opcode(ins);
        for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
            CostSolver::_BBL_instruction_cost[i][bblid] += _instruction_latency[i][opcode];
        }
    }
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
}

CostSolver::CostSolver(const std::string filename)
{
    CostSolver();
    AddControlCost(filename);
}

COST CostSolver::Minimize()
{
    ASSERTX(_BBL_size <= 62);
    UINT64 i = (1 << _BBL_size) - 1;
    CostSolver::DECISION decision;
    CostSolver::DECISION best;
    COST mincost = FLT_MAX;
    for (UINT32 j = 0; j <= _BBL_size; j++) {
        decision.push_back(PIM);
    }
    
    for (; i != (UINT64)(-1); i--) {
        for (UINT32 j = 1; j <= _BBL_size; j++) {
            if ((i >> (j - 1)) & 1)
                decision[j] = PIM;
            else
                decision[j] = CPU;
        }
        COST cost = Cost(decision);
        if (cost < mincost) {
            mincost = cost;
            best = decision;
        }
    }
    for (UINT32 j = 1; j <= _BBL_size; j++)
        std::cout << best[j];
    std::cout << std::endl;
    std::cout << mincost << std::endl;
    return 0;
}

COST CostSolver::Cost(CostSolver::DECISION &decision)
{
    std::set<CostTerm>::iterator it = _cost_term_set.begin();
    std::set<CostTerm>::iterator eit = _cost_term_set.end();
    COST result = 0;
    for (; it != eit; it++) {
        result += it->Cost(decision);
    }
    return result;
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
}

VOID CostSolver::AddCostTerm(const CostTerm &cost) {
    if (cost._coefficient == 0) {
        return;
    }
    std::set<CostTerm>::iterator it = _cost_term_set.find(cost);
    if (it != _cost_term_set.end()) {
        it->_coefficient += cost._coefficient;
    }
    else {
        _cost_term_set.insert(cost);
    }
}

VOID CostSolver::AddControlCost(const std::string filename)
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

    /****************************
    The control cost of BBL i -> BBL j depends on the offloading decision of BBL i and BBL j
    totalcost += cc[0][0]*(1-d[i])*(1-d[j]) + cc[0][1]*(1-d[i])*d[j] + cc[1][0]*d[i]*(1-dec[j]) + cc[1][1]*d[i]*d[j]
    ****************************/

    while(getline(ifs, curline)) {
        std::stringstream ss(curline);
        BBLID head, tail;
        ss >> head;
        while (ss >> tail) {
            std::vector<INT32> list;
            list.push_back(-head);
            list.push_back(-tail);
            AddCostTerm(CostTerm(_control_latency[0][0], list));
            list.clear();
            list.push_back(-head);
            list.push_back(tail);
            AddCostTerm(CostTerm(_control_latency[0][1], list));
            list.clear();
            list.push_back(head);
            list.push_back(-tail);
            AddCostTerm(CostTerm(_control_latency[1][0], list));
            list.clear();
            list.push_back(head);
            list.push_back(tail);
            AddCostTerm(CostTerm(_control_latency[1][1], list));
        }
    }
}

VOID CostSolver::AddInstructionCost(std::vector<COST> (&_BBL_instruction_cost)[MAX_COST_SITE])
{
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        ASSERTX(_BBL_instruction_cost[i].size() == _BBL_size);
    }

    /****************************
    The instruction cost of BBL i depends solely on the offloading decision of BBL i
    totalcost += cc[0]*multiplier[0]*(1-d[i]) + cc[1]*multiplier[1]*d[i] or
    totalcost += (-cc[0]*multiplier[0]+cc[1]*multiplier[1])*d[i] + cc[0]*multiplier[0]
    ****************************/
    for (BBLID i = 0; i < _BBL_size; i++) {
        COST cost = -_BBL_instruction_cost[0][i] * _instruction_multiplier[0] + _BBL_instruction_cost[1][i] * _instruction_multiplier[1];
        AddCostTerm(CostTerm(cost, i));
        cost = _BBL_instruction_cost[0][i] * _instruction_multiplier[0];
        AddCostTerm(CostTerm(cost));
    }
}

VOID CostSolver::AddDataReuseCost(std::vector<BBLOP> *op)
{
    std::vector<BBLOP>::iterator it = op->begin();
    std::vector<BBLOP>::iterator eit = op->end();
    std::vector<INT32> origin;
    std::vector<INT32> flipped;
    std::ofstream ofs("output.txt", std::ofstream::app);
    bool printflag = false;
    for (; it != eit; it++) {
        ofs << it->first << "," << it->second << " -> ";
        printflag = true;
        // BBLID curid = it->first;
        // ACCESS_TYPE curtype = it->second;

        // /****************************
        // The data reuse cost of a LOAD is dependent on the offloading decision of all operations between itself and the most recent STORE. 
        // A PIM LOAD needs to pay the FLUSH cost when the LOADs and the most recent STORE are all on CPU.
        // A CPU LOAD needs to pay the FETCH cost when the LOADs and the most recent STORE are all on PIM.

        // Let the most recent STORE be on BBL i, and the LOAD itself be on BBL j, then:
        // totalcost += clwb*d[j]*(1-d[i])*(1-d[i+1])*...*(1-d[j-1])
        //            + fetch*(1-d[j])*d[i]*d[i+1]*...*d[j-1]

        // If there is no STORE before it,
        // A PIM LOAD does not need to pay any cost.
        // A CPU LOAD needs to pay the FETCH cost.

        // So:
        // totalcost += fetch*(1-d[j])
        // ****************************/
        // if (curtype == ACCESS_TYPE_LOAD) {
        //     if (origin.empty()) { // no STORE
        //         AddCostTerm(CostTerm(_fetch_cost, -curid));
        //         origin.push_back(curid);
        //         flipped.push_back(-curid);
        //     }
        //     else {
        //         flipped.push_back(curid);
        //         AddCostTerm(CostTerm(_clwb_cost, flipped));
        //         flipped.pop_back();
        //         flipped.push_back(-curid);

        //         origin.push_back(-curid);
        //         AddCostTerm(CostTerm(_fetch_cost, origin));
        //         origin.pop_back();
        //         origin.push_back(curid);
        //     }
        // }
        // /****************************
        // The data reuse cost of a STORE is dependent on the offloading decision of all operations between itself and the most recent STORE besides itself. 
        // A PIM STORE needs to pay the INVALIDATE cost when any one among the LOADs and the most recent STORE is on CPU.
        // A CPU STORE needs to pay the FETCH cost when the LOADs and the most recent STORE are all on PIM.

        // Let the most recent STORE be on BBL i, and the LOAD itself be on BBL j, then:
        // totalcost += invalidate*d[j]*(1-d[i]*d[i+1]*...*d[j-1])
        //            + fetch*(1-d[j])*d[i]*d[i+1]*...*d[j-1]

        // If there is no STORE before it,
        // A PIM STORE does not need to pay any cost.
        // A CPU STORE needs to pay the FETCH cost.

        // So:
        // totalcost += fetch*(1-d[j])
        // ****************************/
        // if (curtype == ACCESS_TYPE_STORE) {
        //     if (origin.empty()) { // no STORE
        //         AddCostTerm(CostTerm(_fetch_cost, -curid));
        //         origin.push_back(curid);
        //         flipped.push_back(-curid);
        //     }
        //     else {
        //         AddCostTerm(CostTerm(_invalidate_cost, curid));
        //         origin.push_back(curid);
        //         AddCostTerm(CostTerm(-_invalidate_cost, origin));
        //         origin.pop_back();

        //         origin.push_back(-curid);
        //         AddCostTerm(CostTerm(_fetch_cost, origin));

        //         origin.clear();
        //         origin.push_back(curid);
        //         flipped.clear();
        //         flipped.push_back(-curid);
        //     }
        // }
        // if (curtype == ACCESS_TYPE_NUM) {
        //     ASSERTX(0);
        // }

    }
    if (printflag)
        ofs << std::endl;
}

std::ostream &CostSolver::print(std::ostream &out)
{
    std::set<CostTerm>::iterator it = _cost_term_set.begin();
    std::set<CostTerm>::iterator eit = _cost_term_set.end();
    if (_cost_term_set.size() == 0) return out;
    it->print(out);
    if (_cost_term_set.size() == 1) return out;
    for (it++; it != eit; it++) {
        out << " + ";
        it->print(out);
    }
    return out;
}

/* ===================================================================== */
/* CostSolver::CostTerm */
/* ===================================================================== */

CostSolver::CostTerm::CostTerm(COST c, INT32 id) : _coefficient(c)
{
    AddVar(id);
}

CostSolver::CostTerm::CostTerm(COST c, std::vector<INT32> &v) : _coefficient(c)
{
    AddVar(v);
}

COST CostSolver::CostTerm::Cost(DECISION &decision) const
{
    std::set<INT32>::const_iterator it = _varproduct.begin();
    std::set<INT32>::const_iterator eit = _varproduct.end();
    BOOL result = true;
    for (; it != eit; it++) {
        if (*it > 0)
            result &= decision[*it];
        else
            result &= (!decision[-(*it)]);
    }
    return (_coefficient * result);
}

VOID CostSolver::CostTerm::AddVar(INT32 id)
{
    if (_varproduct.find(-id) != _varproduct.end())
        _coefficient = 0;
    if (_coefficient != 0)
        _varproduct.insert(id);
}

VOID CostSolver::CostTerm::AddVar(std::vector<INT32> &v)
{
    std::vector<INT32>::iterator vit = v.begin();
    std::vector<INT32>::iterator evit = v.end();
    for (; vit != evit; vit++) {
        if (_varproduct.find(-(*vit)) != _varproduct.end())
            _coefficient = 0;
        if (_coefficient != 0)
            _varproduct.insert(*vit);
    }
}

std::ostream &CostSolver::CostTerm::print(std::ostream &out) const
{
    std::set<INT32>::const_iterator it = _varproduct.begin();
    std::set<INT32>::const_iterator eit = _varproduct.end();
    if (_coefficient < 0)
        out << "(" << _coefficient << ")";
    else if (_coefficient > 0)
        out << _coefficient;
    else
        return out;

    if (_varproduct.size() == 0) return out;
    if (*it > 0)
        out << " * d[" << *it << "]";
    else
        out << " * (1 - d[" << -(*it) << "])";
    if (_varproduct.size() == 1) return out;
    for (it++; it != eit; it++) {
        if (*it > 0)
        out << " * d[" << *it << "]";
        else 
            out << " * (1 - d[" << -(*it) << "])";
    }
    return out;
}


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
    std::cout << "BBL\t"
    << "CPUIns\t\t" << "PIMIns\t\t"
    << "CPUMem\t\t" << "PIMMem\t\t"
    << "difference" << std::endl;
    for (UINT32 i = 1; i < CostSolver::_BBL_size; i++) {
        COST difference = (CostSolver::_BBL_instruction_cost[CPU][i] 
            - CostSolver::_BBL_instruction_cost[PIM][i] * 10
            + CostSolver::_BBL_memory_cost[CPU][i] 
            - CostSolver::_BBL_memory_cost[PIM][i]);
        // if (difference != 0) {
            std::cout << i << "\t"
            << CostSolver::_BBL_instruction_cost[CPU][i] << "\t\t"
            << CostSolver::_BBL_instruction_cost[PIM][i] * 10 << "\t\t"
            << CostSolver::_BBL_memory_cost[CPU][i] << "\t\t"
            << CostSolver::_BBL_memory_cost[PIM][i] << "\t\t";
            std::cout << difference << std::endl;
        // }
    }
    std::ofstream ofs("output.dot", std::ofstream::out);
    DataReuse::print(ofs);
}