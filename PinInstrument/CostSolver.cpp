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
void CostSolver::initialize(CommandLineParser *parser)
{
    _command_line_parser = parser;
    _batchthreshold = 0;
    _batchsize = 0;
    std::string line, token;
    uint32_t tid;
    uint64_t hi, lo;
    BBLStats bblstats;
    std::ifstream cpustats(_command_line_parser->cpustatsfile());
    std::ifstream pimstats(_command_line_parser->pimstatsfile());

    // set global BBL with UUID(0, 0) as basic block 0
    _bblhash_map[UUID(0, 0)] = BBLStatsPair();

    while(std::getline(cpustats, line)) {
        std::stringstream ss(line);
        ss >> tid >> std::hex >> hi >> lo 
        >> std::dec >> bblstats.elapsed_time
        >> bblstats.instruction_count >> bblstats.memory_access;

        InsertBBLHash(UUID(hi, lo), bblstats, CostSite::CPU);
    }
    while(std::getline(pimstats, line)) {
        std::stringstream ss(line);
        ss >> tid >> std::hex >> hi >> lo 
        >> std::dec >> bblstats.elapsed_time
        >> bblstats.instruction_count >> bblstats.memory_access;

        if (tid == 0)
            InsertBBLHash(UUID(hi, lo), bblstats, CostSite::PIM);
    }
}

void CostSolver::InsertBBLHash(UUID bblhash, BBLStats bblstats, CostSite site)
{
    // iterator points to a BBLStatsPair
    auto it = _bblhash_map.find(bblhash);
    if (it == _bblhash_map.end()) {
        it = _bblhash_map.insert(std::make_pair(bblhash, BBLStatsPair())).first;
        it->second.bblid = _bblhash_map.size() - 1;
    }
    it->second.bblstats[site] += bblstats;
}

CostSolver::DECISION CostSolver::PrintMPKISolution(std::ostream &out)
{
    std::vector<std::pair<UUID, BBLStatsPair>> sorted_hash(_bblhash_map.begin(), _bblhash_map.end());
    std::sort(sorted_hash.begin(), sorted_hash.end(),
        [](auto &a, auto &b) { return a.second.bblid < b.second.bblid; });

    DECISION decision;

    uint64_t pim_total_instr = 0;
    for (auto it = sorted_hash.begin(); it != sorted_hash.end(); ++it) {
        pim_total_instr += it->second.bblstats[CostSite::PIM].instruction_count;
    }

    uint64_t instr_threshold = pim_total_instr * 0.01;
    uint64_t mpki_threshold = 10;
    uint64_t cpu_only_time = 0, pim_only_time = 0, mpki_time = 0, mpki_cpu_time = 0, mpki_pim_time = 0;
    int i = 0;
    for (auto it = sorted_hash.begin(); it != sorted_hash.end(); ++it) {
        BBLStats &cpustats = it->second.bblstats[CostSite::CPU];
        BBLStats &pimstats = it->second.bblstats[CostSite::PIM];
        double instr = pimstats.instruction_count;
        double mem = pimstats.memory_access;
        double mpki = mem / instr * 1000.0;

        cpu_only_time += cpustats.elapsed_time;
        pim_only_time += pimstats.elapsed_time;

        // deal with the part that is not inside any BBL
        uint64_t global_val = -1;
        if (it->first.first == global_val && it->first.second == global_val) {
            mpki_time += cpustats.elapsed_time;
            mpki_cpu_time += cpustats.elapsed_time;
            decision.push_back(CostSite::CPU);
            continue;
        }

        if (mpki > mpki_threshold && instr > instr_threshold) {
            mpki_time += pimstats.elapsed_time;
            mpki_pim_time += pimstats.elapsed_time;
            decision.push_back(CostSite::PIM);
        }
        else {
            mpki_time += cpustats.elapsed_time;
            mpki_cpu_time += cpustats.elapsed_time;
            decision.push_back(CostSite::CPU);
        }
    }
    out << "CPU only time (ns): " << cpu_only_time << std::endl
    << "PIM only time (ns): " << pim_only_time << std::endl
    << "MPKI offloading time (ns): " << mpki_time << " = CPU " << mpki_cpu_time << " + PIM " << mpki_pim_time << std::endl;

    PrintDecision(out, sorted_hash, decision, false);

    return decision;
}

CostSolver::DECISION CostSolver::PrintSolution(std::ostream &out)
{
    DECISION decision;
    
    if (_command_line_parser->mode() == "mpki") {
        decision = PrintMPKISolution(out);
    }

    return decision;
}

std::ostream &CostSolver::PrintDecision(std::ostream &out, std::vector<std::pair<UUID, BBLStatsPair>> sorted_hash, const DECISION &decision, bool toscreen)
{
    out << "========================================================" << std::endl;
    if (toscreen == true) {
        for (uint32_t i = 0; i < _bblhash_map.size(); i++) {
            out << i << ":"
                << (decision[i] == CPU ? "C" : "")
                << (decision[i] == PIM ? "P" : "")
                << " ";
        }
        out << std::endl;
    }
    else {
        out << std::right << std::setw(7) << "BBLID"
            << std::right << std::setw(10) << "Decision"
            << std::right << std::setw(15) << "CPU"
            << std::right << std::setw(15) << "PIM"
            << std::right << std::setw(15) << "difference"
            << std::right << std::setw(18) << "Hash(hi)"
            << std::right << std::setw(18) << "Hash(lo)"
            << std::endl;
        for (uint32_t i = 0; i < sorted_hash.size(); i++) {
            if (sorted_hash[i].second.bblid != i) {
                std::cout << "Incorrect bblid: " << sorted_hash[i].second.bblid << " " << i << std::endl;
                assert(0);
            }
            BBLStats &cpustats = sorted_hash[i].second.bblstats[CostSite::CPU];
            BBLStats &pimstats = sorted_hash[i].second.bblstats[CostSite::PIM];
            int64_t diff = (int64_t)cpustats.elapsed_time - (int64_t)pimstats.elapsed_time;
            out << std::right << std::setw(7) << i
                << std::right << std::setw(10) << (decision[i] == PIM ? "P" : "C")
                << std::right << std::setw(15) << cpustats.elapsed_time
                << std::right << std::setw(15) << pimstats.elapsed_time
                << std::right << std::setw(15) << diff
                << "  "
                << std::setfill('0') << std::setw(16) << std::hex << sorted_hash[i].first.first
                << "  "
                << std::setfill('0') << std::setw(16) << std::hex << sorted_hash[i].first.second
                << std::setfill(' ') << std::dec
                << std::endl;
        }
    }
    return out;
}


// CostSolver::DECISION CostSolver::PrintSolution(std::ostream &out)
// {
//     SetBBLSize(_cost_package->_bbl_size);
//     // set partial total
//     for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
//         for (uint32_t j = 0; j < _cost_package->_bbl_size; j++) {
//             _BBL_partial_total[i][j]
//                 = _cost_package->BBLInstructionCost((CostSite)i, j)
//                 + _cost_package->BBLMemoryCost((CostSite)i, j);
//         }
//     }

//     std::cout << std::right << std::setw(14) << "PLAN"
//               << std::right << std::setw(15) << "INSTRUCTION"
//               << std::right << std::setw(15) << "MEMORY"
//               << std::right << std::setw(15) << "INS_MEM"
//               << std::right << std::setw(15) << "PARTIAL"
//               << std::right << std::setw(15) << "REUSE"
//               << std::right << std::setw(15) << "TOTAL"
//               << std::endl;
//     out << std::right << std::setw(14) << "PLAN"
//               << std::right << std::setw(15) << "INSTRUCTION"
//               << std::right << std::setw(15) << "MEMORY"
//               << std::right << std::setw(15) << "INS_MEM"
//               << std::right << std::setw(15) << "PARTIAL"
//               << std::right << std::setw(15) << "REUSE"
//               << std::right << std::setw(15) << "TOTAL"
//               << std::endl;

// #ifdef PIMPROF_MPKI
//     DECISION _mpki_decision;
//     infomsg() << "bblid\tmiss\tinstr\tmpki\tsimd" << std::endl;
//     for (uint32_t i = 0; i < _cost_package->_bbl_size; i++) {
//         FLT64 mpki = (FLT64)_cost_package->_cache_miss[i] / _cost_package->_bbl_instr_cnt[i] * 1000;
//         infomsg() << i << "\t" << _cost_package->_cache_miss[i] << "\t" << _cost_package->_bbl_instr_cnt[i] << "\t" << mpki << "\t" << _cost_package->_simd_instr_cnt[i] << std::endl;
//         if (mpki >= _mpkithreshold) {
//             _mpki_decision.push_back(PIM);
//         }
//         else {
//             _mpki_decision.push_back(CPU);
//         }
//     }
//     PrintDecisionStat(std::cout, _mpki_decision, "MPKI");
//     PrintDecisionStat(out, _mpki_decision, "MPKI");
// #endif

//     // pure CPU
//     DECISION _pure_cpu_decision;
//     for (uint32_t i = 0; i < _cost_package->_bbl_size; i++) {
//         _pure_cpu_decision.push_back(CPU);
//     }
//     PrintDecisionStat(std::cout, _pure_cpu_decision, "Pure CPU");
//     PrintDecisionStat(out, _pure_cpu_decision, "Pure CPU");

//      // pure PIM
//     DECISION _pure_pim_decision;
//     for (uint32_t i = 0; i < _cost_package->_bbl_size; i++) {
//         _pure_pim_decision.push_back(PIM);
//     }
//     PrintDecisionStat(std::cout, _pure_pim_decision, "Pure PIM");
//     PrintDecisionStat(out, _pure_pim_decision, "Pure PIM");


//     // greedy decision
//     DECISION _greedy_decision;
//     for (uint32_t i = 0; i < _cost_package->_bbl_size; i++) {
//         if (_BBL_partial_total[CPU][i] <= _BBL_partial_total[PIM][i]) {
//             _greedy_decision.push_back(CPU);
//         }
//         else {
//             _greedy_decision.push_back(PIM);
//         }
//     }
//     // std::ofstream tempofs("greedy_decision.out", std::ofstream::out);
//     // PrintDecision(tempofs, decision, false);
//     PrintDecisionStat(std::cout, _greedy_decision, "Greedy");
//     PrintDecisionStat(out, _greedy_decision, "Greedy");

//     // Optimal
//     DECISION _opt_decision = FindOptimal();
//     PrintDecisionStat(std::cout, _opt_decision, "PIMProf opt");
//     PrintDecisionStat(out, _opt_decision, "PIMProf opt");
//     out << std::endl;

// #ifdef PIMPROF_MPKI
//      out << std::right << std::setw(14) << "PLAN"
//           << std::right << std::setw(15) << "INSTRUCTION"
//           << std::right << std::setw(15) << "L1I"
//           << std::right << std::setw(15) << "L1D"
//           << std::right << std::setw(15) << "L2"
//           << std::right << std::setw(15) << "L3"
//           << std::right << std::setw(15) << "MEMORY"
//           << std::endl;
//      PrintCostBreakdown(out, _pure_cpu_decision, "Pure CPU");
//      PrintCostBreakdown(out, _pure_pim_decision, "Pure PIM");
//      PrintCostBreakdown(out, _greedy_decision, "Greedy");
//      PrintCostBreakdown(out, _opt_decision, "PIMProf opt");
//      out << std::endl;
// #endif

//     PrintDecision(out, _opt_decision, false);

//     if (!_cost_package->_roi_decision.empty()) {
//         out << std::endl;
//         // decision based on ROI
//         PrintDecisionStat(std::cout, _cost_package->_roi_decision, "ROIDecision");
//         PrintDecisionStat(out, _cost_package->_roi_decision, "ROI decision");
//         out << std::endl;
//         PrintDecision(out, _cost_package->_roi_decision, false);
//     }

//     return _opt_decision;
// }

// void CostSolver::TrieBFS(COST &cost, const CostSolver::DECISION &decision, BBLID bblid, TrieNode *root, bool isDifferent)
// {
//     if (root->_isLeaf) {
//         if (isDifferent) {
//             // If the initial W is on CPU and there are subsequent R/W on PIM,
//             // then this segment contributes to a flush of CPU and data fetch from PIM.
//             // We conservatively assume that the fetch will promote data to L1
//             if (decision[bblid] == CPU) {
//                 // if the initial W can be parallelized, then we assume that
//                 // the data corresponding to the chain can be flushed/fetched in parallel
//                 if (_cost_package->_bbl_parallelizable[bblid])
//                     cost += root->_count * (_flush_cost[CPU] / _cost_package->_core_count[CPU] + _fetch_cost[PIM] / _cost_package->_core_count[PIM]);
//                 else
//                     cost += root->_count * (_flush_cost[CPU] + _fetch_cost[PIM]);
//             }
//             // If the initial W is on PIM and there are subsequent R/W on CPU,
//             // then this segment contributes to a flush of PIM and data fetch from CPU
//             else {
//                 if (_cost_package->_bbl_parallelizable[bblid])
//                     cost += root->_count * (_flush_cost[PIM] / _cost_package->_core_count[PIM] + _fetch_cost[CPU] / _cost_package->_core_count[CPU]);
//                 else
//                     cost += root->_count * (_flush_cost[PIM] + _fetch_cost[CPU]);
//             }
//         }
//     }
//     else {
//         std::map<BBLID, TrieNode *>::iterator it = root->_children.begin();
//         std::map<BBLID, TrieNode *>::iterator eit = root->_children.end();
//         for (; it != eit; it++) {
//             if (isDifferent) {
//                 TrieBFS(cost, decision, it->first, it->second, true);
//             }
//             else if (decision[bblid] != decision[it->first]) {
//                 TrieBFS(cost, decision, it->first, it->second, true);
//             }
//             else {
//                 TrieBFS(cost, decision, it->first, it->second, false);
//             }
//         }
//     }
// }

// COST CostSolver::Cost(const CostSolver::DECISION &decision, TrieNode *reusetree)
// {
//     COST cur_reuse_cost = 0;
//     COST cur_instr_cost = 0;
//     COST cur_mem_cost = 0;
//     std::map<BBLID, TrieNode *>::iterator it = reusetree->_children.begin();
//     std::map<BBLID, TrieNode *>::iterator eit = reusetree->_children.end();
//     for (; it != eit; it++) {
//         TrieBFS(cur_reuse_cost, decision, it->first, it->second, false);
//     }
//     for (uint32_t i = 0; i < _cost_package->_bbl_size; i++) {
//         CostSite site = decision[i];
//         if (site == CPU || site == PIM) {
//             cur_instr_cost += _cost_package->BBLInstructionCost(site, i);
//             cur_mem_cost += _cost_package->BBLMemoryCost(site, i);
//         }
//     }
//     return (cur_reuse_cost + cur_instr_cost + cur_mem_cost);
// }

// bool CostSolverComparator(const TrieNode *l, const TrieNode *r)
// {
//     return l->_count > r->_count;
// }

// CostSolver::DECISION CostSolver::FindOptimal()
// {
//     std::sort(_cost_package->_data_reuse.getLeaves().begin(), _cost_package->_data_reuse.getLeaves().end(), CostSolverComparator);

//     COST reuse_max = 0;
//     for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
//         reuse_max = std::max(reuse_max, _flush_cost[i]);
//         reuse_max = std::max(reuse_max, _fetch_cost[i]);
//     }

//     COST cur_total = FLT_MAX;
//     int seg_count = INT_MAX;
//     DECISION decision;

//     //initialize all decision to INVALID
//     for (uint32_t i = 0; i < _cost_package->_bbl_size; i++) {
//         decision.push_back(INVALID);
//     }

//     TrieNode *partial_root = new TrieNode();
//     DataReuseSegment allidset;
//     int currentnode = 0;
//     int leavessize = _cost_package->_data_reuse.getLeaves().size();

//     for (int i = 0; currentnode < leavessize; i++) {
//         std::cout << "batch " << i << std::endl;
//         std::vector<BBLID> idvec;
//         // insert segments until the number of different BBLs hit _batchsize
//         while (currentnode < leavessize) {
//             DataReuseSegment seg;
//             _cost_package->_data_reuse.ExportSegment(seg, _cost_package->_data_reuse.getLeaves()[currentnode]);
//             std::vector<BBLID> diff = seg.diff(allidset);
//             // std::cout << idvec.size() << " " << diff.size() << std::endl;
//             if (idvec.size() + diff.size() > (unsigned)_batchsize) break;
//             allidset.insert(seg);
//             idvec.insert(idvec.end(), diff.begin(), diff.end());
//             _cost_package->_data_reuse.UpdateTrie(partial_root, seg);
//             currentnode++;
//             seg_count = seg.getCount();
//         }

//         int idvecsize = idvec.size();
//         for (int j = 0; j < idvecsize; j++) {
//             std::cout << idvec[j] << " "; 
//         }
//         std::cout << std::endl;

//         // find optimal in this batch
//         assert(idvecsize <= _batchsize);
//         uint64_t permute = (1 << idvecsize) - 1;

//         // should not compare the cost between batches, so reset cur_total
//         cur_total = FLT_MAX;

//         DECISION temp_decision = decision;
//         for (; permute != (uint64_t)(-1); permute--) {
//             for (int j = 0; j < idvecsize; j++) {
//                 if ((permute >> j) & 1)
//                     temp_decision[idvec[j]] = PIM;
//                 else
//                     temp_decision[idvec[j]] = CPU;
//             }
//             COST temp_total = Cost(temp_decision, partial_root);
//             if (temp_total < cur_total) {
//                 cur_total = temp_total;
//                 decision = temp_decision;
//             }
//             // PrintDecision(std::cout, decision, true);
//         }
//         std::cout << "cur_total = " << cur_total << std::endl;
//         std::cout << seg_count << " " << reuse_max << " " << cur_total << std::endl;
//         if (seg_count * reuse_max < _batchthreshold * cur_total) break;
//     }
//     // std::ofstream ofs("temp.dot", std::ofstream::out);
//     // _cost_package->_data_reuse.print(ofs, partial_root);
//     // ofs.close();

//     _cost_package->_data_reuse.DeleteTrie(partial_root);

//     for (uint32_t i = 0; i < _cost_package->_bbl_size; i++) {
//         if (decision[i] == INVALID) {
//             if (_BBL_partial_total[CPU][i] <= _BBL_partial_total[PIM][i]) {
//                 decision[i] = CPU;
//             }
//             else {
//                 decision[i] = PIM;
//             }
//         }
//     }
//     // for (uint32_t i = 0; i < _cost_package->_bbl_size; i++) {
//     //     if (decision[i] == INVALID)
//     //         decision[i] = PIM;
//     // }

//     cur_total = Cost(decision, _cost_package->_data_reuse.getRoot());
//     // iterate over the remaining BBs 2 times until convergence
//     for (int j = 0; j < 2; j++) {
//         for (uint32_t i = 0; i < _cost_package->_bbl_size; i++) {
//             BBLID id = i;

//             if (decision[id] == CPU) {
//                 decision[id] = PIM;
//                 COST temp_total = Cost(decision, _cost_package->_data_reuse.getRoot());
//                 if (temp_total > cur_total)
//                     decision[id] = CPU;
//                 else
//                     cur_total = temp_total;
//             }
//             else {
//                 decision[id] = CPU;
//                 COST temp_total = Cost(decision, _cost_package->_data_reuse.getRoot());
//                 if (temp_total > cur_total)
//                     decision[id] = PIM;
//                 else {
//                     cur_total = temp_total;
//                 }
//             }
//         }
//         std::cout << "cur_total = " << cur_total << std::endl;
//     }

//     return decision;
// }

// void CostSolver::ReadConfig(ConfigReader &reader)
// {
//     for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
//         COST ilp = reader.GetReal("ILP", CostSiteName[i], -1);
//         assert(ilp > 0);
//         _cost_package->_ilp[i] = ilp;
//     }

//     for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
//         COST mlp = reader.GetReal("MLP", CostSiteName[i], -1);
//         assert(mlp > 0);
//         _cost_package->_mlp[i] = mlp;
//     }

//     for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
//         uint32_t core = reader.GetInteger("Core", CostSiteName[i], -1);
//         assert(core > 0);
//         _cost_package->_core_count[i] = core;
//     }

//     for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
//         uint32_t multiplier = reader.GetInteger("SIMDCapability", CostSiteName[i], -1);
//         assert(multiplier > 0);
//         _cost_package->_simd_capability[i] = multiplier;
//     }

//     for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
//         for (uint32_t j = 0; j < MAX_COST_SITE; j++) {
//             std::string coststr = CostSiteName[i] + "to" + CostSiteName[j];
//             COST cost = reader.GetReal("UnitControlCost", coststr, -1);
//             if (cost >= 0) {
//                 _cost_package->_control_latency[i][j] = cost;
//             }
//         }
//     }

//     for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
//         COST cost = reader.GetReal("CacheFlushCost", CostSiteName[i], -1);
//         assert(cost >= 0);
//         _flush_cost[i] = cost;
//     }

//     for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
//         _fetch_cost[i] = 0;
//         for (uint32_t j = 0; j < MAX_LEVEL - 1; j++) {
//             std::string name = CostSiteName[i] + "/" + StorageLevelName[j];
//             auto sections = reader.Sections();
//             if (sections.find(name) == sections.end()) {
//                 break;
//             }
//             // TODO: for simplicity, use IL1 latency only
//             if (j == 1) continue;

//             COST cost = reader.GetReal(name, "hitcost", -1);
//             assert(cost >= 0);
//             _fetch_cost[i] += cost;
//         }
//         // memory
//         std::string name = CostSiteName[i] + "/" + StorageLevelName[MEM];
//         COST cost = reader.GetReal(name, "hitcost", -1);
//         assert(cost >= 0);
//         _fetch_cost[i] += cost;
//     }

//     for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
//         COST cost = reader.GetReal("UnitInstructionCost", CostSiteName[i], -1);
//         assert(cost >= 0);
//         _cost_package->_instruction_multiplier[i] = cost;
//     }

//     double threshold = reader.GetReal("DataReuse", "BatchThreshold", -1);
//     assert(threshold >= 0);
//     _batchthreshold = threshold;

//     int size = reader.GetInteger("DataReuse", "BatchSize", -1);
//     assert(size > 0);
//     _batchsize = size;

//     int mpki = reader.GetInteger("Other", "MPKIThreshold", -1);
//     assert(mpki > 0);
//     _mpkithreshold = mpki;
// }

// void CostSolver::SetBBLSize(BBLID bbl_size) {
//     for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
//         _BBL_partial_total[i].resize(bbl_size);
//         memset(&_BBL_partial_total[i][0], 0, bbl_size * sizeof(_BBL_partial_total[i][0]));
//     }
// #ifdef PIMPROF_MPKI
//     for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
//         for (uint32_t j = 0; j < MAX_LEVEL; j++) {
//             _BBL_storage_partial_total[i][j].resize(bbl_size);
//             memset(&_BBL_storage_partial_total[i][j][0], 0, bbl_size * sizeof(_BBL_storage_partial_total[i][j][0]));
//         }
//     }
// #endif
// }


// std::ostream &CostSolver::PrintDecisionStat(std::ostream &out, const DECISION &decision, const std::string &name)
// {
//     COST cur_reuse_cost = 0;
//     COST cur_instr_cost = 0;
//     COST cur_mem_cost = 0;
//     COST cur_ins_mem_cost = 0;
//     std::map<BBLID, TrieNode *>::iterator it = _cost_package->_data_reuse.getRoot()->_children.begin();
//     std::map<BBLID, TrieNode *>::iterator eit = _cost_package->_data_reuse.getRoot()->_children.end();
//     for (; it != eit; it++) {
//         TrieBFS(cur_reuse_cost, decision, it->first, it->second, false);
//     }
//     for (uint32_t i = 0; i < _cost_package->_bbl_size; i++) {
//         CostSite site = decision[i];
//         if (site == CPU || site == PIM) {
//             cur_instr_cost += _cost_package->BBLInstructionCost(site, i);
//             cur_mem_cost += _cost_package->BBLMemoryCost(site, i);
//             cur_ins_mem_cost += _cost_package->BBLInstructionMemoryCost(site, i);
//         }
//     }

//     out << std::right << std::setw(14) << name + ":"
//         << std::right << std::setw(15) << cur_instr_cost 
//         << std::right << std::setw(15) << cur_mem_cost
//         << std::right << std::setw(15) << cur_ins_mem_cost
//         << std::right << std::setw(15) << (cur_instr_cost + cur_mem_cost) 
//         << std::right << std::setw(15) << cur_reuse_cost
//         << std::right << std::setw(15) << (cur_instr_cost + cur_mem_cost + cur_reuse_cost)
//         << std::endl;
//     return out;
// }

// std::ostream &CostSolver::PrintCostBreakdown(std::ostream &out, const DECISION &decision, const std::string &name)
// {
//     COST cur_instr_cost = 0;
//     COST cur_storage_level_cost[MAX_LEVEL] = {0};
//     for (uint32_t i = 0; i < _cost_package->_bbl_size; i++) {
//         CostSite site = decision[i];
//         if (site == CPU || site == PIM) {
//             cur_instr_cost += _cost_package->BBLInstructionCost(site, i);
//         }
//     }

//     for (uint32_t i = 0; i < MAX_LEVEL; i++) {
//         for (uint32_t j = 0; j < _cost_package->_bbl_size; j++) {
//             CostSite site = decision[j];
//             if (site == CPU || site == PIM) {
//                 cur_storage_level_cost[i] += _cost_package->BBLStorageLevelCost(site, (StorageLevel)i, j);
//             }
//         }
//     }

//     out << std::right << std::setw(14) << name + ":"
//         << std::right << std::setw(15) << cur_instr_cost 
//         << std::right << std::setw(15) << cur_storage_level_cost[IL1]
//         << std::right << std::setw(15) << cur_storage_level_cost[DL1]
//         << std::right << std::setw(15) << cur_storage_level_cost[UL2]
//         << std::right << std::setw(15) << cur_storage_level_cost[UL3]
//         << std::right << std::setw(15) << cur_storage_level_cost[MEM]
//         << std::endl;
//     return out;
// }

// std::ostream &CostSolver::PrintAnalytics(std::ostream &out)
// {
// #ifdef PIMPROF_MPKI
//     uint64_t total = 0;
//     uint64_t total_visit = 0;
//     for (uint32_t i = 0; i < _cost_package->_bbl_size; i++) {
//         total += _cost_package->_bbl_instr_cnt[i];
//         total_visit += _cost_package->_bbl_visit_cnt[i];
//     }

//     out << "avg instruction in BB: "  << total << " " << total_visit << " " << ((double)total / total_visit) << std::endl;
//     out << std::endl;

//     out << std::right << std::setw(8) << "opcode"
//               << std::right << std::setw(15) << "name"
//               << std::right << std::setw(20) << "cnt"
//               << std::right << std::setw(15) << "CPUCost"
//               << std::right << std::setw(15) << "PIMCost"
//               << std::endl;
//     for (uint32_t i = 0; i < MAX_INDEX; i++) {
//         if (_cost_package->_type_instr_cnt[i] > 0) {
//             out << std::right << std::setw(8) << i
//                   << std::right << std::setw(15) << OPCODE_StringShort(i)
//                   << std::right << std::setw(20) << _cost_package->_type_instr_cnt[i]
//                   << std::right << std::setw(15) << _cost_package->_type_instr_cost[CPU][i]
//                   << std::right << std::setw(15) << _cost_package->_type_instr_cost[PIM][i]
//                   << std::endl;
//         }
//     }
//     out << std::endl;

//     out << "total instr: " << _cost_package->_total_instr_cnt << std::endl;
//     out << "total simd instr: " << _cost_package->_total_simd_instr_cnt << std::endl;

//     out << "CPU simd cost: " << _cost_package->_total_simd_cost[CPU] << std::endl;
//     out << "PIM simd cost: " << _cost_package->_total_simd_cost[PIM] << std::endl;
// #endif
//     // std::vector<std::vector<uint32_t>> cdftemp;
//     // std::vector<uint32_t> cdf; 
//     // for (uint32_t i = 0; i < _cost_package->_bbl_size; i++) {
//     //     uint32_t per = _cost_package->_bbl_instr_cnt[i] / _cost_package->_bbl_visit_cnt[i];
//     //     // assert(_cost_package->_bbl_instr_cnt[i] % _cost_package->_bbl_visit_cnt[i] == 0);
//     //     for (; cdf.size() <= per; cdf.push_back(0));
//     //     cdf[per] += _cost_package->_bbl_visit_cnt[i];

//     //     for (; cdftemp.size() <= per; cdftemp.push_back(std::vector<uint32_t>()));
//     //     cdftemp[per].push_back(i);
//     // }
//     // for (uint32_t i = 0; i < cdf.size(); i++) {
//     //     if (cdf[i] > 0) {
//     //         out << i << " ";
//     //         for (uint32_t j = 0; j < cdftemp[i].size(); j++)
//     //             out << cdftemp[i][j] << " ";
//     //         out << std::endl;
//     //     }
//     // }
//     // out << std::endl;
//     // for (uint32_t i = 0; i < cdf.size(); i++) {
//     //     if (cdf[i] > 0)
//     //     out << i << " " << cdf[i] << std::endl;
//     // }
//     // out << std::endl;
//     // for (uint32_t i = 0; i < cdftemp[cdf.size() - 1].size(); i++) {
//     //     BBLID bblid = cdftemp[cdf.size() - 1][i];
//     //     infomsg() << cdf.size() << " " << bblid << std::endl;
//     //     auto &map = _cost_package->_bbl_hash;
//     //     for (auto it = map.begin(); it != map.end(); ++it) {
//     //         if (it->second == bblid) {
//     //             out << (int64_t)it->first.first << " " << (int64_t)it->first.second << std::endl;
//     //             break;
//     //         }
//     //     } 
//     // }
//     return out;
// }
