//===- CostSolver.cpp - Utils for instrumentation ------------*- C++ -*-===//
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

#include "CostSolver.h"

using namespace PIMProf;

/* ===================================================================== */
/* Static data structure */
/* ===================================================================== */

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

/* ===================================================================== */
/* Global data structure */
/* ===================================================================== */
extern long long int instr_cnt, mem_instr_cnt, nonmem_instr_cnt;

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

std::ostream &CostSolver::PrintBBLDecisionStat(std::ostream &out, const DECISION &decision, bool toscreen)
{
    out << "BBL\t" << "Decision\t"
    << "CPUIns\t\t" << "PIMIns\t\t"
    << "CPUMem\t\t" << "PIMMem\t\t"
    << "difference" << std::endl;
    for (UINT32 i = 0; i < CostSolver::_BBL_size; i++) {
        out << i << "\t"
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
    return out;
}