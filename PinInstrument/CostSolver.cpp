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

    std::cout << std::left << std::setw(14) << "PLAN"
              << std::left << std::setw(15) << "INSTRUCTION"
              << std::left << std::setw(15) << "MEMORY"
              << std::left << std::setw(15) << "PARTIAL"
              << std::left << std::setw(15) << "REUSE"
              << std::left << std::setw(15) << "TOTAL"
              << std::endl;
    out << std::left << std::setw(14) << "PLAN"
              << std::left << std::setw(15) << "INSTRUCTION"
              << std::left << std::setw(15) << "MEMORY"
              << std::left << std::setw(15) << "PARTIAL"
              << std::left << std::setw(15) << "REUSE"
              << std::left << std::setw(15) << "TOTAL"
              << std::endl;

    //
    decision.clear();
    for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
        FLT64 mpki = (FLT64)_cost_package->_cache_miss[i] / _cost_package->_instr_cnt[i] * 1000;
        infomsg() << i << " " << _cost_package->_cache_miss[i] << " " << _cost_package->_instr_cnt[i] << " " << mpki << std::endl;
        if (mpki >= 10) {
            decision.push_back(PIM);
        }
        else {
            decision.push_back(CPU);
        }
    }
    PrintDecisionStat(std::cout, decision, "MPKI");
    PrintDecisionStat(out, decision, "MPKI");
    // pure CPU
    decision.clear();
    for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
        decision.push_back(CPU);
    }
    PrintDecisionStat(std::cout, decision, "Pure CPU");
    PrintDecisionStat(out, decision, "Pure CPU");

     // pure PIM
    decision.clear();
    for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
        decision.push_back(PIM);
    }
    PrintDecisionStat(std::cout, decision, "Pure PIM");
    PrintDecisionStat(out, decision, "Pure PIM");

    // greedy decision
    decision.clear();
    for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
        if (_BBL_partial_total[CPU][i] <= _BBL_partial_total[PIM][i]) {
            decision.push_back(CPU);
        }
        else {
            decision.push_back(PIM);
        }
    }
    // std::ofstream tempofs("greedy_decision.out", std::ofstream::out);
    // PrintDecision(tempofs, decision, false);
    PrintDecisionStat(std::cout, decision, "Greedy");
    PrintDecisionStat(out, decision, "Greedy");

    // Optimal
    result = FindOptimal();

    PrintDecisionStat(std::cout, result, "PIMProf opt");
    PrintDecisionStat(out, result, "PIMProf opt");
    out << std::endl;
    PrintDecision(out, result, false);
    PrintDecision(infomsg(), result, true);

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
    // std::ofstream ofs("temp.dot", std::ofstream::out);
    // _cost_package->_data_reuse.print(ofs, partial_root);
    // ofs.close();

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
            out << i << ":"
                << (decision[i] == CPU ? "C" : "")
                << (decision[i] == PIM ? "P" : "")
                << " ";
        }
        out << std::endl;
    }
    else {
        std::vector<std::pair<UUID, UINT32>> sorted_hash(_cost_package->_bbl_hash.begin(), _cost_package->_bbl_hash.end());
        std::sort(sorted_hash.begin(), sorted_hash.end(),
            [](auto &a, auto &b) { return a.second < b.second; });
        out << std::left << std::setw(7) << "BBLID" 
            << std::left << std::setw(10) << "Decision" 
            << std::left << std::setw(7) << "isomp"
            << std::left << std::setw(15) << "CPUIns" 
            << std::left << std::setw(15) << "PIMIns"
            << std::left << std::setw(15) << "CPUMem" 
            << std::left << std::setw(15) << "PIMMem"
            << std::left << std::setw(15) << "difference"
            << std::left << std::setw(18) << "Hash(hi)" 
            << std::left << std::setw(16) << "Hash(lo)"
            << std::endl;
        for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
            ASSERTX(sorted_hash[i].second == i);
            out << std::left << std::setw(7) << i
                << std::left << std::setw(10) << (decision[i] == PIM ? "P" : "C")
                << std::left << std::setw(7) << (_cost_package->_inOpenMPRegion[i] ? "O" : "X")
                << std::left << std::setw(15) << _cost_package->BBLInstructionCost(CPU, i)
                << std::left << std::setw(15) << _cost_package->BBLInstructionCost(PIM, i)
                << std::left << std::setw(15) << _cost_package->BBLMemoryCost(CPU, i)
                << std::left << std::setw(15) << _cost_package->BBLMemoryCost(PIM, i)
                << std::left << std::setw(15) << _BBL_partial_total[CPU][i] - _BBL_partial_total[PIM][i]
                << std::setfill('0') << std::setw(16) << std::hex << sorted_hash[i].first.first
                << "  "
                << std::setfill('0') << std::setw(16) << std::hex << sorted_hash[i].first.second
                << std::setfill(' ') << std::dec
                << std::endl;
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

    out << std::left << std::setw(14) << name + ":"
        << std::left << std::setw(15) << cur_instr_cost 
        << std::left << std::setw(15) << cur_mem_cost
        << std::left << std::setw(15) << (cur_instr_cost + cur_mem_cost) 
        << std::left << std::setw(15) << cur_reuse_cost
        << std::left << std::setw(15) << (cur_instr_cost + cur_mem_cost + cur_reuse_cost)
        << std::endl;
    return out;
}

std::ostream &CostSolver::PrintAnalytics(std::ostream &out)
{
    UINT64 total = 0;
    UINT64 total_visit = 0;
    for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
        total += _cost_package->_instr_cnt[i];
        total_visit += _cost_package->_bbl_visit_cnt[i];
    }
    std::cout << "total instr: " << _cost_package->_total_instr_cnt << std::endl;

    std::cout << "avg instruction in BB: "  << total << " " << total_visit << " " << ((double)total / total_visit) << std::endl;

    std::vector<std::vector<UINT32>> cdftemp;
    std::vector<UINT32> cdf; 
    for (UINT32 i = 0; i < _cost_package->_bbl_size; i++) {
        UINT32 per = _cost_package->_instr_cnt[i] / _cost_package->_bbl_visit_cnt[i];
        // ASSERTX(_cost_package->_instr_cnt[i] % _cost_package->_bbl_visit_cnt[i] == 0);
        for (; cdf.size() <= per; cdf.push_back(0));
        cdf[per] += _cost_package->_bbl_visit_cnt[i];

        for (; cdftemp.size() <= per; cdftemp.push_back(std::vector<UINT32>()));
        cdftemp[per].push_back(i);
    }
    for (UINT32 i = 0; i < cdf.size(); i++) {
        if (cdf[i] > 0) {
            out << i << " ";
            for (UINT32 j = 0; j < cdftemp[i].size(); j++)
                out << cdftemp[i][j] << " ";
            out << std::endl;
        }
    }
    out << std::endl;
    for (UINT32 i = 0; i < cdf.size(); i++) {
        if (cdf[i] > 0)
        out << i << " " << cdf[i] << std::endl;
    }
    out << std::endl;
    for (UINT32 i = 0; i < cdftemp[cdf.size() - 1].size(); i++) {
        BBLID bblid = cdftemp[cdf.size() - 1][i];
        infomsg() << cdf.size() << " " << bblid << std::endl;
        auto &map = _cost_package->_bbl_hash;
        for (auto it = map.begin(); it != map.end(); ++it) {
            if (it->second == bblid) {
                out << (INT64)it->first.first << " " << (INT64)it->first.second << std::endl;
                break;
            }
        } 
    }
    return out;
}
