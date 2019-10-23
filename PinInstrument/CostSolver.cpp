//===- CostSolver.cpp - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//


#include "CostSolver.h"

using namespace PIMProf;

/* ===================================================================== */
/* CostSolver */
/* ===================================================================== */
void CostSolver::initialize(CostPackage *cost_package, ConfigReader &reader)
{
    _cost_package = cost_package;
    memset(_cost_package->_control_latency, 0, sizeof(_cost_package->_control_latency));

    _cost_package->_instruction_multiplier[PIM] = 1;
    _cost_package->_instruction_multiplier[CPU] = 1;
    _clwb_cost = 0;
    _invalidate_cost = 0;
    _fetch_cost = 0;
    _batchcount = 0;
    _batchsize = 0;

    ReadConfig(reader);
}


CostSolver::DECISION CostSolver::PrintSolution(std::ostream &out)
{
    SetBBLSize(_cost_package->_bbl_size);
    // set partial total
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < _cost_package->_bbl_size; j++) {
            _BBL_partial_total[i][j]
                = _cost_package->BBLInstructionCost((CostSite)i, j)
                + _cost_package->BBLMemoryCost((CostSite)i, j);
        }
    }

    DECISION decision, result;

    std::cout << "instrcnt: " << _cost_package->_mem_instr_cnt << " & " << _cost_package->_nonmem_instr_cnt << " / " << _cost_package->_instr_cnt << std::endl;
    std::cout << "PLAN\t\tINSTRUCTION\tMEMORY\t\tPARTIAL\t\tREUSE\t\tTOTAL" << std::endl;

    // greedy decision
    for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
        if (_BBL_partial_total[CPU][i] <= _BBL_partial_total[PIM][i]) {
            decision.push_back(CPU);
        }
        else {
            decision.push_back(PIM);
        }
    }
    std::ofstream tempofs("greedy_decision.out", std::ofstream::out);
    PrintDecision(tempofs, decision, false);
    PrintDecisionStat(std::cout, decision, "Pure greedy");

    // Optimal
    result = FindOptimal();
    PrintDecision(infomsg(), result, true);
    PrintDecision(out, result, false);
    PrintDecisionStat(std::cout, result, "PIMProf opt");

    // pure CPU
    decision.clear();
    for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
        decision.push_back(CPU);
    }
    PrintDecisionStat(std::cout, decision, "Pure CPU");

     // pure PIM
    decision.clear();
    for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
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
    for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
        CostSite site = decision[i];
        if (site == CPU || site == PIM) {
            cur_instr_cost += _cost_package->BBLInstructionCost(site, i);
            cur_mem_cost += _cost_package->BBLMemoryCost(site, i);
        }
    }
    return (cur_reuse_cost + cur_instr_cost + cur_mem_cost);
}

bool CostSolverComparator(const TrieNode *l, const TrieNode *r)
{
    return l->_count > r->_count;
}

CostSolver::DECISION CostSolver::FindOptimal()
{
    std::sort(_cost_package->_data_reuse.getLeaves().begin(), _cost_package->_data_reuse.getLeaves().end(), CostSolverComparator);

    COST cur_total = FLT_MAX;
    DECISION decision;

    //initialize all decision to INVALID
    for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
        decision.push_back(INVALID);
    }

    TrieNode *partial_root = new TrieNode();
    DataReuseSegment allidset;
    int currentnode = 0;
    int leavessize = _cost_package->_data_reuse.getLeaves().size();

    for (int i = 0; i < _batchcount; i++) {
        std::cout << "cnt" << i << std::endl;
        
        std::vector<BBLID> idvec;
        // insert segments until the number of different BBLs hit _batchsize
        while (currentnode < leavessize) {
            DataReuseSegment seg;
            _cost_package->_data_reuse.ExportSegment(seg, _cost_package->_data_reuse.getLeaves()[currentnode]);
            std::vector<BBLID> diff = seg.diff(allidset);
            // std::cout << idvec.size() << " " << diff.size() << std::endl;
            if (idvec.size() + diff.size() > (unsigned)_batchsize) break;
            allidset.insert(seg);
            idvec.insert(idvec.end(), diff.begin(), diff.end());
            _cost_package->_data_reuse.UpdateTrie(partial_root, seg);
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
    _cost_package->_data_reuse.print(ofs, partial_root);
    ofs.close();

    _cost_package->_data_reuse.DeleteTrie(partial_root);

    for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
        if (decision[i] == INVALID) {
            if (_BBL_partial_total[CPU][i] <= _BBL_partial_total[PIM][i]) {
                decision[i] = CPU;
            }
            else {
                decision[i] = PIM;
            }
        }
    }
    // for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
    //     if (decision[i] == INVALID)
    //         decision[i] = PIM;
    // }

    cur_total = Cost(decision, _cost_package->_data_reuse.getRoot());
    std::cout << cur_total << std::endl;
    // iterate over the remaining BBs 10 times until convergence
    for (int j = 0; j < 3; j++) {
        for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
            BBLID id = i;
            
            if (decision[id] == CPU) {
                decision[id] = PIM;
                COST temp_total = Cost(decision, _cost_package->_data_reuse.getRoot());
                if (temp_total > cur_total)
                    decision[id] = CPU;
                else
                    cur_total = temp_total;
            }
            else {
                decision[id] = CPU;
                COST temp_total = Cost(decision, _cost_package->_data_reuse.getRoot());
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

VOID CostSolver::ReadConfig(ConfigReader &reader)
{
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        COST ilp = reader.GetInteger("ILP", CostSiteName[i], -1);
        ASSERTX(ilp > 0);
        _cost_package->_ilp[i] = ilp;
    }

    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        COST mlp = reader.GetInteger("MLP", CostSiteName[i], -1);
        ASSERTX(mlp > 0);
        _cost_package->_mlp[i] = mlp;
    }

    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        UINT32 core = reader.GetInteger("Core", CostSiteName[i], -1);
        ASSERTX(core > 0);
        _cost_package->_core_count[i] = core;
    }

    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < MAX_COST_SITE; j++) {
            std::string coststr = CostSiteName[i] + "to" + CostSiteName[j];
            COST cost = reader.GetReal("UnitControlCost", coststr, -1);
            if (cost >= 0) {
                _cost_package->_control_latency[i][j] = cost;
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
        _cost_package->_instruction_multiplier[i] = cost;
    }

    int size = reader.GetInteger("DataReuse", "BatchCount", -1);
    ASSERTX(size >= 0);
    _batchcount = size;

    size = reader.GetInteger("DataReuse", "BatchSize", -1);
    ASSERTX(size > 0);
    _batchsize = size;
}

VOID CostSolver::SetBBLSize(BBLID bbl_size) {
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        _BBL_partial_total[i].resize(bbl_size);
        memset(&_BBL_partial_total[i][0], 0, bbl_size * sizeof _BBL_partial_total[i][0]);
    }
}

std::ostream &CostSolver::PrintDecision(std::ostream &out, const DECISION &decision, bool toscreen)
{
    if (toscreen == true) {
        for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
            out << i << ":";
            out << (decision[i] == CPU ? "C" : "");
            out << (decision[i] == PIM ? "P" : "");
            out << " ";
        }
        out << std::endl;
    }
    else {
        for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
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
    std::map<BBLID, TrieNode *>::iterator it = _cost_package->_data_reuse.getRoot()->_children.begin();
    std::map<BBLID, TrieNode *>::iterator eit = _cost_package->_data_reuse.getRoot()->_children.end();
    for (; it != eit; it++) {
        TrieBFS(cur_reuse_cost, decision, it->first, it->second, false);
    }
    for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
        CostSite site = decision[i];
        if (site == CPU || site == PIM) {
            cur_instr_cost += _cost_package->BBLInstructionCost(site, i);
            cur_mem_cost += _cost_package->BBLMemoryCost(site, i);
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
    out << "BBL\t" << "isomp\t" << "Decision\t"
    << "CPUIns\t\t" << "PIMIns\t\t"
    << "CPUMem\t\t" << "PIMMem\t\t"
    << "difference" << std::endl;
    for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
        out << i << "\t"
        << (decision[i] == CPU ? "C" : "") 
        << (decision[i] == PIM ? "P" : "") << "\t"
        << (_cost_package->_inOpenMPRegion[i] ? "O" : "X") << "\t"
        << _cost_package->BBLInstructionCost(CPU, i)
        << "\t\t"
        << _cost_package->BBLInstructionCost(PIM, i)
        << "\t\t"
        << _cost_package->BBLMemoryCost(CPU, i) << "\t\t"
        << _cost_package->BBLMemoryCost(PIM, i) << "\t\t"
        << _BBL_partial_total[CPU][i] - _BBL_partial_total[PIM][i] << std::endl;
    }
    return out;
}