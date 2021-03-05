//===- CostSolver.cpp - Utils for instrumentation ---------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include <cfloat>
#include <climits>

#include "Common.h"
#include "CostSolver.h"

using namespace PIMProf;

/* ===================================================================== */
/* CostSolver */
/* ===================================================================== */
void CostSolver::initialize(CommandLineParser *parser)
{
    _command_line_parser = parser;
    _batch_threshold = 0;
    _batch_size = 0;

    std::ifstream cpustats(_command_line_parser->cpustatsfile());
    std::ifstream pimstats(_command_line_parser->pimstatsfile());
    std::ifstream reuse(_command_line_parser->reusefile());
    assert(cpustats.is_open());
    assert(pimstats.is_open());
    assert(reuse.is_open());
    ParseStats(cpustats, _bblhash2stats[CPU]);
    ParseStats(pimstats, _bblhash2stats[PIM]);

    ParseReuse(reuse, _data_reuse);

    // temporarily define flush and fetch cost here
    _flush_cost[CostSite::CPU] = 60;
    _flush_cost[CostSite::PIM] = 30;
    _fetch_cost[CostSite::CPU] = 60;
    _fetch_cost[CostSite::PIM] = 30;
    _switch_cost[CostSite::CPU] = 800;
    _switch_cost[CostSite::PIM] = 800;
    _mpki_threshold = 80;
    _batch_threshold = 0.001;
    _batch_size = 10;
}

CostSolver::~CostSolver()
{
    for (int i = 0; i < MAX_COST_SITE; i++) {
        for (auto it : _bblhash2stats[i]) {
            delete it.second;
        }
    }
}

const std::vector<ThreadBBLStats *>* CostSolver::getSorted()
{
    if (_dirty) {

        SortStatsMap(_bblhash2stats[CPU], _sortedstats[CPU]);
        // align CPU with PIM
        for (auto elem : _sortedstats[CPU]) {
            UUID bblhash = elem->bblhash;
            BBLID bblid = elem->bblid;
            auto p = _bblhash2stats[PIM].find(bblhash);
            if (p != _bblhash2stats[PIM].end()) {
                _sortedstats[PIM].push_back(p->second);
                p->second->bblid = bblid;
            }
            else {
                // create placeholder
                ThreadBBLStats *stats = new ThreadBBLStats(0, BBLStats(bblid, bblhash));
                _bblhash2stats[PIM].insert(std::make_pair(bblhash, stats));
                _sortedstats[PIM].push_back(stats);
            }
        }

        assert(_sortedstats[CPU].size() == _sortedstats[PIM].size());
        for (BBLID i = 0; i < _sortedstats[CPU].size(); i++) {
            assert(_sortedstats[CPU][i]->bblid == _sortedstats[PIM][i]->bblid);
            assert(_sortedstats[CPU][i]->bblhash == _sortedstats[PIM][i]->bblhash);
        }
        _dirty = false;
    }
    return _sortedstats;
}

void CostSolver::ParseStats(std::istream &ifs, UUIDHashMap<ThreadBBLStats *> &statsmap)
{
    std::string line, token;
    int tid = 0;
    while(std::getline(ifs, line)) {
        if (line.find(HORIZONTAL_LINE) != std::string::npos) { // skip next 2 lines
            std::getline(ifs, line);
            std::stringstream ss(line);
            ss >> token >> tid;
            std::getline(ifs, line);
            continue;
        }
        std::stringstream ss(line);

        BBLStats bblstats;
        ss >> bblstats.bblid
           >> bblstats.elapsed_time
           >> bblstats.instruction_count
           >> bblstats.memory_access
           >> std::hex >> bblstats.bblhash.first >> bblstats.bblhash.second;
        assert(bblstats.elapsed_time >= 0);
        auto it = statsmap.find(bblstats.bblhash);
        if (statsmap.find(bblstats.bblhash) == statsmap.end()) {
            ThreadBBLStats *p = new ThreadBBLStats(tid, bblstats);
            statsmap.insert(std::make_pair(bblstats.bblhash, p));
        }
        else {
            it->second->MergeStats(tid, bblstats);
        }
    }
}

void CostSolver::PrintStats(std::ostream &ofs)
{
    const std::vector<ThreadBBLStats *> *sorted = getSorted();

    for (int i = 0; i < MAX_COST_SITE; ++i) {
        ofs << HORIZONTAL_LINE << std::endl;
        ofs << std::setw(7) << "BBLID"
            << std::setw(15) << "Time(ns)"
            << std::setw(15) << "Instruction"
            << std::setw(15) << "Memory Access"
            << std::setw(18) << "Hash(hi)"
            << std::setw(18) << "Hash(lo)"
            << std::endl;
        for (auto it = sorted[i].begin(); it != sorted[i].end(); ++it)
        {
            UUID bblhash = (*it)->bblhash;
            ofs << std::setw(7) << (*it)->bblid
                << std::setw(15) << (*it)->elapsed_time
                << std::setw(15) << (*it)->instruction_count
                << std::setw(15) << (*it)->memory_access
                << "  " << std::hex
                << std::setfill('0') << std::setw(16) << bblhash.first
                << "  "
                << std::setfill('0') << std::setw(16) << bblhash.second
                << std::setfill(' ') << std::dec << std::endl;
        }
    }
    
}

void CostSolver::ParseReuse(std::istream &ifs, DataReuse<BBLID> &reuse)
{
    std::string line, token;

    // we parses reuse segments and BBL switch counts at the same time
    bool isreusesegment = true; 
    
    while(std::getline(ifs, line)) {
        if (line.find(HORIZONTAL_LINE) != std::string::npos) {
            std::getline(ifs, line);
            std::stringstream ss(line);
            ss >> token;
            if (token == "ReuseSegment") {
                isreusesegment = true;
            }
            else if (token == "BBLSwitchCount") {
                isreusesegment = false;
            }
            else { assert(0); }
            continue;
        }
        if (isreusesegment) {
            std::stringstream ss(line);
            BBLIDDataReuseSegment seg;
            ss >> token >> token >> token;
            BBLID head = std::stoi(token.substr(0, token.size() - 1));
            int64_t count;
            ss >> token >> token >> count;
            ss >> token;
            BBLID bblid;
            while (ss >> bblid) {
                seg.insert(bblid);
            }
            seg.setHead(head);
            if (count < 0) {
                errormsg("count < 0 for line ``%s''", line.c_str());
                assert(count >= 0);
            }
            seg.setCount(count);
            reuse.UpdateTrie(reuse.getRoot(), &seg);
        }
        else {
            std::stringstream ss(line);
            BBLID fromidx;
            ss >> token >> token >> fromidx >> token;
            std::vector<std::pair<BBLID, uint64_t>> toidxvec;
            while (ss >> token) {
                size_t delim = token.find(':');
                BBLID toidx = stoi(token.substr(0, delim));
                uint64_t count = stoi(token.substr(delim + 1));
                toidxvec.push_back(std::make_pair(toidx, count));
            }
            _bbl_switch_count.RowInsert(fromidx, toidxvec);
        }
    }
    _bbl_switch_count.Sort();

    // std::ofstream ofs("graph.dot", std::ios::out);
    // reuse.PrintDotGraph(ofs, [](BBLID bblid){ return bblid; });

    // reuse.PrintAllSegments(std::cout, [](BBLID bblid){ return bblid; });
}

DECISION CostSolver::PrintSolution(std::ostream &ofs)
{
    DECISION decision;
    
    if (_command_line_parser->mode() == CommandLineParser::Mode::MPKI) {
        ofs << "CPU only time (ns): " << ElapsedTime(CPU) << std::endl
            << "PIM only time (ns): " << ElapsedTime(PIM) << std::endl;
        decision = PrintMPKIStats(ofs);
    }
    if (_command_line_parser->mode() == CommandLineParser::Mode::REUSE) {
        ofs << "CPU only time (ns): " << ElapsedTime(CPU) << std::endl
            << "PIM only time (ns): " << ElapsedTime(PIM) << std::endl;

        const std::vector<ThreadBBLStats *> *sorted = getSorted();
        uint64_t instr_cnt = 0;
        for (int i = 0; i < sorted[CPU].size(); i++) {
            instr_cnt += sorted[CPU][i]->instruction_count;
        }
        ofs << "Instruction " << instr_cnt << std::endl;
        PrintMPKIStats(ofs);
        PrintGreedyStats(ofs);
        decision = PrintReuseStats(ofs);
    }
    if (_command_line_parser->mode() == CommandLineParser::Mode::DEBUG) {
        ofs << "CPU only time (ns): " << ElapsedTime(CPU) << std::endl
            << "PIM only time (ns): " << ElapsedTime(PIM) << std::endl;
        decision = Debug_ConsiderSwitchCost(ofs);
    }

    PrintDecision(ofs, decision, false);

    return decision;
}

std::ostream & CostSolver::PrintDecision(std::ostream &ofs, const DECISION &decision, bool toscreen)
{
    const std::vector<ThreadBBLStats *> *sorted = getSorted();
    ofs << HORIZONTAL_LINE << std::endl;
    if (toscreen == true) {
        for (uint32_t i = 0; i < decision.size(); i++) {
            ofs << i << ":"
                << getCostSiteString(decision[i])
                << " ";
        }
        ofs << std::endl;
    }
    else {
        ofs << std::setw(7) << "BBLID"
            << std::setw(10) << "Decision"
            << std::setw(15) << "CPU"
            << std::setw(15) << "PIM"
            << std::setw(15) << "difference"
            << std::setw(18) << "Hash(hi)"
            << std::setw(18) << "Hash(lo)"
            << std::endl;
        for (uint32_t i = 0; i < sorted[CPU].size(); i++) {
            auto *cpustats = sorted[CPU][i];
            auto *pimstats = sorted[PIM][i];
            COST diff = cpustats->MaxElapsedTime() - pimstats->MaxElapsedTime();
            ofs << std::setw(7) << i
                << std::setw(10) << getCostSiteString(decision[i])
                << std::setw(15) << cpustats->MaxElapsedTime()
                << std::setw(15) << pimstats->MaxElapsedTime()
                << std::setw(15) << diff
                << "  " << std::hex
                << std::setfill('0') << std::setw(16) << cpustats->bblhash.first
                << "  "
                << std::setfill('0') << std::setw(16) << cpustats->bblhash.second
                << std::setfill(' ') << std::dec << std::endl;
        }
    }
    return ofs;
}

DECISION CostSolver::PrintMPKIStats(std::ostream &ofs)
{
    const std::vector<ThreadBBLStats *> *sorted = getSorted();
    DECISION decision;
    uint64_t pim_total_instr = 0;
    for (auto it = sorted[PIM].begin(); it != sorted[PIM].end(); ++it) {
        pim_total_instr += (*it)->instruction_count;
    }

    uint64_t instr_threshold = pim_total_instr * 0.01;
    
    for (BBLID i = 0; i < sorted[CPU].size(); ++i) {
        auto *cpustats = sorted[CPU][i];
        auto *pimstats = sorted[PIM][i];

        double instr = pimstats->instruction_count;
        double mem = pimstats->memory_access;
        double mpki = mem / instr * 1000.0;

        // deal with the part that is not inside any BBL
        if (cpustats->bblhash == GLOBAL_BBLHASH) {
            decision.push_back(CostSite::CPU);
            continue;
        }

        if (mpki > _mpki_threshold && instr > instr_threshold) {
            decision.push_back(CostSite::PIM);
        }
        else {
            decision.push_back(CostSite::CPU);
        }
    }

    COST reuse_cost = ReuseCost(decision, _data_reuse.getRoot());
    COST switch_cost = SwitchCost(decision, &_bbl_switch_count);
    auto elapsed_time = ElapsedTime(decision);
    COST total_time = reuse_cost + switch_cost + elapsed_time.first + elapsed_time.second;
    assert(total_time == Cost(decision, _data_reuse.getRoot(), &_bbl_switch_count));

    ofs << "MPKI offloading time (ns): " << total_time << " = CPU " << elapsed_time.first << " + PIM " << elapsed_time.second << " + REUSE " << reuse_cost << " + SWITCH " << switch_cost << std::endl;

    return decision;
}

DECISION CostSolver::PrintGreedyStats(std::ostream &ofs)
{
    const std::vector<ThreadBBLStats *> *sorted = getSorted();
    DECISION decision;
    for (BBLID i = 0; i < sorted[CPU].size(); ++i) {
        auto *cpustats = sorted[CPU][i];
        auto *pimstats = sorted[PIM][i];
        if (cpustats->MaxElapsedTime() <= pimstats->MaxElapsedTime()) {
            decision.push_back(CPU);
        }
        else {
            decision.push_back(PIM);
        }
    }
    COST reuse_cost = ReuseCost(decision, _data_reuse.getRoot());
    COST switch_cost = SwitchCost(decision, &_bbl_switch_count);
    auto elapsed_time = ElapsedTime(decision);
    COST total_time = reuse_cost + switch_cost + elapsed_time.first + elapsed_time.second;
    assert(total_time == Cost(decision, _data_reuse.getRoot(), &_bbl_switch_count));

    ofs << "Greedy offloading time (ns): " << total_time << " = CPU " << elapsed_time.first << " + PIM " << elapsed_time.second << " + REUSE " << reuse_cost << " + SWITCH " << switch_cost << std::endl;

    return decision;
}


// this function does not check whether there is duplicate BBLID in cur_batch
COST CostSolver::PermuteDecision(DECISION &decision, const std::vector<BBLID> &cur_batch, const BBLIDTrieNode *partial_root)
{

    int cur_batch_size = cur_batch.size();
    assert(cur_batch_size < 64);
    COST cur_total = FLT_MAX;
    DECISION temp_decision = decision;
    // find optimal in this batch
    uint64_t permute = (1 << cur_batch_size) - 1;
    
    for (; permute != (uint64_t)(-1); permute--) {
        for (int j = 0; j < cur_batch_size; j++) {
            if ((permute >> j) & 1)
                temp_decision[cur_batch[j]] = PIM;
            else
                temp_decision[cur_batch[j]] = CPU;
        }
        // PrintDecision(std::cout, temp_decision, true);
        COST temp_total = Cost(temp_decision, partial_root, &_bbl_switch_count);
        if (temp_total < cur_total) {
            cur_total = temp_total;
            decision = temp_decision; 
        }
    }

    return cur_total;
}

DECISION CostSolver::PrintReuseStats(std::ostream &ofs)
{
    _data_reuse.SortLeaves();

    COST reuse_max = SingleSegMaxReuseCost();

    //initialize all decision to INVALID
    DECISION decision;
    decision.resize(_bblhash2stats[CPU].size(), INVALID);
    COST cur_total = FLT_MAX;
    int seg_count = INT_MAX;

    BBLIDTrieNode *partial_root = new BBLIDTrieNode();
    BBLIDDataReuseSegment allidset;
    int cur_node = 0;
    int leaves_size = _data_reuse.getLeaves().size();

    int batch_cnt = 0;
    while(cur_node < leaves_size) {
        std::vector<BBLID> cur_batch;
        // insert segments until the number of different BBLs exceeds _batch_size or all nodes are added
        while (cur_node < leaves_size) {
            BBLIDDataReuseSegment seg;
            _data_reuse.ExportSegment(&seg, _data_reuse.getLeaves()[cur_node]);
            std::vector<BBLID> diff = seg.diff(allidset);
            // std::cout << cur_batch.size() << " " << diff.size() << std::endl;
            cur_node++;
            // ignore too long segments
            if (diff.size() > (unsigned)_batch_size) continue;
            
            allidset.insert(seg);
            cur_batch.insert(cur_batch.end(), diff.begin(), diff.end());
            _data_reuse.UpdateTrie(partial_root, &seg);

            seg_count = seg.getCount();

            if (cur_batch.size() + diff.size() > (unsigned)_batch_size) break;
        }

        std::cout << "batch = " << batch_cnt << ", size = " << cur_batch.size() << std::endl;

        cur_total = PermuteDecision(decision, cur_batch, partial_root);

        for (auto elem : cur_batch) {
            std::cout << elem << getCostSiteString(decision[elem]) << " ";
        }

        std::cout << "seg_count = " << seg_count << ", reuse_max = " << reuse_max << ", cur_total = " << cur_total << std::endl;
        std::cout << std::endl;
        if (seg_count * reuse_max < _batch_threshold * cur_total) break;
        batch_cnt++;
    }
    // std::ofstream ofs("temp.dot", std::ofstream::out);
    // _cost_package->_data_reuse.print(ofs, partial_root);
    // ofs.close();

    _data_reuse.DeleteTrie(partial_root);

    const std::vector<ThreadBBLStats *> *sorted = getSorted();

    // assign decision for BBLs that did not occur in the reuse chains
    for (BBLID i = 0; i < sorted[CPU].size(); ++i) {
        auto *cpustats = sorted[CPU][i];
        auto *pimstats = sorted[PIM][i];

        if (decision[i] == INVALID) {
            if (cpustats->MaxElapsedTime() <= pimstats->MaxElapsedTime()) {
                decision[i] = CPU;
            }
            else {
                decision[i] = PIM;
            }
        }
    }

    cur_total = Cost(decision, _data_reuse.getRoot(), &_bbl_switch_count);
    std::cout << "cur_total = " << cur_total << std::endl;
    // iterate over the remaining BBs 5 times until convergence
    for (int j = 0; j < 2; ++j) {
        for (BBLID id = 0; id < sorted[CPU].size(); id++) {
            // swap decision[id] and check if it reduces overhead
            decision[id] = (decision[id] == CPU ? PIM : CPU);
            COST temp_total = Cost(decision, _data_reuse.getRoot(), &_bbl_switch_count);
            if (temp_total > cur_total) {
                decision[id] = (decision[id] == CPU ? PIM : CPU);
            }
            else {
                cur_total = temp_total;
            }
        }
        std::cout << "cur_total = " << cur_total << std::endl;
    }

    COST reuse_cost = ReuseCost(decision, _data_reuse.getRoot());
    COST switch_cost = SwitchCost(decision, &_bbl_switch_count);
    auto elapsed_time = ElapsedTime(decision);
    COST total_time = reuse_cost + switch_cost + elapsed_time.first + elapsed_time.second;
    assert(total_time == Cost(decision, _data_reuse.getRoot(), &_bbl_switch_count));

    ofs << "Reuse offloading time (ns): " << total_time << " = CPU " << elapsed_time.first << " + PIM " << elapsed_time.second << " + REUSE " << reuse_cost << " + SWITCH " << switch_cost << std::endl;

    return decision;
}

void CostSolver::PrintDisjointSets(std::ostream &ofs)
{
    DisjointSet ds;
    _data_reuse.SortLeaves();

    COST elapsed_time_min = (ElapsedTime(CPU) < ElapsedTime(PIM) ? ElapsedTime(CPU) : ElapsedTime(PIM));
    COST reuse_max = SingleSegMaxReuseCost();

    for (auto i : _data_reuse.getLeaves()) {
        BBLIDDataReuseSegment seg;
        _data_reuse.ExportSegment(&seg, i);
        BBLID first = *seg.begin();
        for (auto elem : seg) {
            ds.Union(first, elem);
        }
        if (seg.getCount() * reuse_max < _batch_threshold * elapsed_time_min) break;
    }

    for (auto it : ds.parent) {
        ofs << it.first << " " << it.second << std::endl;
    }
    
    for (auto it : ds.parent) {
        if (ds.Find(it.first) == it.first) {
            BBLID head = it.first;
            int count = 0;
            for (auto it2 : ds.parent) {
                if (ds.Find(it2.first) == head) {
                    ofs << it2.first << " ";
                     count++;
                }
            }
            ofs << " | Count = " << count << std::endl;
        }
    }
}

DECISION CostSolver::Debug_StartFromUnimportantSegment(std::ostream &ofs)
{
    _data_reuse.SortLeaves();
    // std::ofstream oo("sortedsegments.out", std::ofstream::out);
    // _data_reuse.PrintAllSegments(oo, CostSolver::_get_id);
    // oo << std::endl;
    // _data_reuse.PrintBBLOccurrence(oo, CostSolver::_get_id);


    COST elapsed_time_min = (ElapsedTime(CPU) < ElapsedTime(PIM) ? ElapsedTime(CPU) : ElapsedTime(PIM));
    COST reuse_max = SingleSegMaxReuseCost();

    //initialize all decision to INVALID
    DECISION decision;
    decision.resize(_bblhash2stats[CPU].size(), INVALID);
    COST cur_total = FLT_MAX;

    BBLIDTrieNode *partial_root = new BBLIDTrieNode();
    BBLIDDataReuseSegment allidset;
    int cur_node = 0;
    int leaves_size = _data_reuse.getLeaves().size();

    // find out the node with smallest importance but exceeds the threshold, skip the rest
    while (cur_node < leaves_size) {
        BBLIDDataReuseSegment seg;
        _data_reuse.ExportSegment(&seg, _data_reuse.getLeaves()[cur_node]);
        if (seg.getCount() * reuse_max < _batch_threshold * elapsed_time_min) break;
        cur_node++;
    }

    for (; cur_node >= 0; --cur_node) {
        BBLIDDataReuseSegment seg;
        _data_reuse.ExportSegment(&seg, _data_reuse.getLeaves()[cur_node]);
        _data_reuse.UpdateTrie(partial_root, &seg);
        std::vector<BBLID> cur_batch(seg.begin(), seg.end());
        std::cout << "cur_node = " << cur_node << ", size = " << seg.size() << std::endl;

        // ignore too long segments
        if (seg.size() >= _batch_size) continue;

        cur_total = PermuteDecision(decision, cur_batch, partial_root);
        
        for (auto elem : cur_batch) {
            std::cout << elem << getCostSiteString(decision[elem]) << " ";
        }

        std::cout << "seg_count = " << seg.getCount() << ", reuse_max = " << reuse_max << ", cur_total = " << cur_total << std::endl;
        std::cout << std::endl;
    }

    _data_reuse.DeleteTrie(partial_root);

    const std::vector<ThreadBBLStats *> *sorted = getSorted();

    // assign decision for BBLs that did not occur in the reuse chains
    for (BBLID i = 0; i < sorted[CPU].size(); ++i) {
        auto *cpustats = sorted[CPU][i];
        auto *pimstats = sorted[PIM][i];

        if (decision[i] == INVALID) {
            if (cpustats->MaxElapsedTime() <= pimstats->MaxElapsedTime()) {
                decision[i] = CPU;
            }
            else {
                decision[i] = PIM;
            }
        }
    }

    cur_total = Cost(decision, _data_reuse.getRoot(), &_bbl_switch_count);
    std::cout << "cur_total = " << cur_total << std::endl;
    // iterate over the remaining BBs until convergence
    for (int j = 0; j < 2; j++) {
        for (BBLID id = 0; id < sorted[CPU].size(); id++) {
            // swap decision[id] and check if it reduces overhead
            decision[id] = (decision[id] == CPU ? PIM : CPU);
            COST temp_total = Cost(decision, _data_reuse.getRoot(), &_bbl_switch_count);
            if (temp_total > cur_total) {
                decision[id] = (decision[id] == CPU ? PIM : CPU);
            }
            else {
                cur_total = temp_total;
            }
        }
        std::cout << "cur_total = " << cur_total << std::endl;
    }

    COST reuse_cost = ReuseCost(decision, _data_reuse.getRoot());
    COST switch_cost = SwitchCost(decision, &_bbl_switch_count);
    auto elapsed_time = ElapsedTime(decision);
    COST total_time = reuse_cost + switch_cost + elapsed_time.first + elapsed_time.second;
    assert(total_time == Cost(decision, _data_reuse.getRoot(), &_bbl_switch_count));

    ofs << "Reuse offloading time (ns): " << total_time << " = CPU " << elapsed_time.first << " + PIM " << elapsed_time.second << " + REUSE " << reuse_cost << " + SWITCH " << switch_cost << std::endl;


    std::ofstream oo(
        (_command_line_parser->outputfile() + ".debug").c_str(),
        std::ofstream::out);
    _bbl_switch_count.printSwitch(oo, decision, _switch_cost);

    return decision;
}

DECISION CostSolver::Debug_ConsiderSwitchCost(std::ostream &ofs)
{
    _data_reuse.SortLeaves();
    // std::ofstream oo("sortedsegments.out", std::ofstream::out);
    // _data_reuse.PrintAllSegments(oo, CostSolver::_get_id);
    // oo << std::endl;
    // _data_reuse.PrintBBLOccurrence(oo, CostSolver::_get_id);


    COST elapsed_time_min = (ElapsedTime(CPU) < ElapsedTime(PIM) ? ElapsedTime(CPU) : ElapsedTime(PIM));
    COST reuse_max = SingleSegMaxReuseCost();

    //initialize all decision to INVALID
    DECISION decision;
    decision.resize(_bblhash2stats[CPU].size(), INVALID);
    COST cur_total = FLT_MAX;

    BBLIDTrieNode *partial_root = new BBLIDTrieNode();
    BBLIDDataReuseSegment allidset;
    int cur_node = 0;
    int leaves_size = _data_reuse.getLeaves().size();

    // find out the node with smallest importance but exceeds the threshold, skip the rest
    while (cur_node < leaves_size) {
        BBLIDDataReuseSegment seg;
        _data_reuse.ExportSegment(&seg, _data_reuse.getLeaves()[cur_node]);
        if (seg.getCount() * reuse_max < _batch_threshold * elapsed_time_min) break;
        cur_node++;
    }

    for (; cur_node >= 0; --cur_node) {
        BBLIDDataReuseSegment seg;
        _data_reuse.ExportSegment(&seg, _data_reuse.getLeaves()[cur_node]);
        _data_reuse.UpdateTrie(partial_root, &seg);

        // ignore too long segments
        if (seg.size() >= _batch_size) continue;

        // find BBLs with most occurence in all switching points related to BBLs in current segment
        std::unordered_map<BBLID, uint64_t> total_switch_cnt_map;
        for (auto fromidx : seg) {
            BBLSwitchCountList<BBLID>::BBLSwitchCountRow &row = _bbl_switch_count.getRow(fromidx);
            for (auto elem : row) {
                BBLID toidx = elem.first;
                uint64_t count = elem.second;
                auto it = total_switch_cnt_map.find(toidx);
                if (it != total_switch_cnt_map.end()) {
                    it->second += count;
                }
                else {
                    total_switch_cnt_map[toidx] = count;
                }
            }
        }
        std::vector<std::pair<BBLID, uint64_t>> total_switch_cnt_vec(total_switch_cnt_map.begin(), total_switch_cnt_map.end());
        std::sort(total_switch_cnt_vec.begin(), total_switch_cnt_vec.end(),
            [](auto l, auto r){ return l.second > r.second; });

        for (auto elem : total_switch_cnt_vec) {
            if (seg.size() >= _batch_size) break;
            seg.insert(elem.first);
        }

        std::vector<BBLID> cur_batch(seg.begin(), seg.end());
        std::cout << "cur_node = " << cur_node << ", size = " << seg.size() << std::endl;

        cur_total = PermuteDecision(decision, cur_batch, partial_root);
        
        for (auto elem : cur_batch) {
            std::cout << elem << getCostSiteString(decision[elem]) << " ";
        }
 

        std::cout << "seg_count = " << seg.getCount() << ", reuse_max = " << reuse_max << ", cur_total = " << cur_total << std::endl;
        std::cout << std::endl;
    }

    _data_reuse.DeleteTrie(partial_root);

    const std::vector<ThreadBBLStats *> *sorted = getSorted();

    // assign decision for BBLs that did not occur in the reuse chains
    for (BBLID i = 0; i < sorted[CPU].size(); ++i) {
        auto *cpustats = sorted[CPU][i];
        auto *pimstats = sorted[PIM][i];

        if (decision[i] == INVALID) {
            if (cpustats->MaxElapsedTime() <= pimstats->MaxElapsedTime()) {
                decision[i] = CPU;
            }
            else {
                decision[i] = PIM;
            }
        }
    }

    cur_total = Cost(decision, _data_reuse.getRoot(), &_bbl_switch_count);
    std::cout << "cur_total = " << cur_total << std::endl;
    // iterate over the remaining BBs until convergence
    for (int j = 0; j < 2; j++) {
        for (BBLID id = 0; id < sorted[CPU].size(); id++) {
            // swap decision[id] and check if it reduces overhead
            decision[id] = (decision[id] == CPU ? PIM : CPU);
            COST temp_total = Cost(decision, _data_reuse.getRoot(), &_bbl_switch_count);
            if (temp_total > cur_total) {
                decision[id] = (decision[id] == CPU ? PIM : CPU);
            }
            else {
                cur_total = temp_total;
            }
        }
        std::cout << "cur_total = " << cur_total << std::endl;
    }

    COST reuse_cost = ReuseCost(decision, _data_reuse.getRoot());
    COST switch_cost = SwitchCost(decision, &_bbl_switch_count);
    auto elapsed_time = ElapsedTime(decision);
    COST total_time = reuse_cost + switch_cost + elapsed_time.first + elapsed_time.second;

    ofs << "Reuse offloading time (ns): " << total_time << " = CPU " << elapsed_time.first << " + PIM " << elapsed_time.second << " + REUSE " << reuse_cost << " + SWITCH " << switch_cost << std::endl;


    std::ofstream oo(
        (_command_line_parser->outputfile() + ".debug").c_str(),
        std::ofstream::out);
    _bbl_switch_count.printSwitch(oo, decision, _switch_cost);

    return decision;
}

COST CostSolver::Cost(const DECISION &decision, const BBLIDTrieNode *reusetree, const BBLIDBBLSwitchCountList *switchcnt)
{
    auto pair = ElapsedTime(decision);
    return (ReuseCost(decision, reusetree) + SwitchCost(decision, switchcnt) + pair.first + pair.second);
}

COST CostSolver::ElapsedTime(CostSite site)
{
    const std::vector<ThreadBBLStats *> *sorted = getSorted();
    COST elapsed_time = 0;
    for (BBLID i = 0; i < sorted[site].size(); ++i) {
        auto *stats = sorted[site][i];
        elapsed_time += stats->MaxElapsedTime();
    }
    return elapsed_time;
}

std::pair<COST, COST> CostSolver::ElapsedTime(const DECISION &decision)
{
    COST cpu_elapsed_time = 0, pim_elapsed_time = 0;
    const std::vector<ThreadBBLStats *> *sorted = getSorted();
    for (uint32_t i = 0; i < sorted[CPU].size(); i++) {
        auto *cpustats = sorted[CPU][i];
        auto *pimstats = sorted[PIM][i];
        if (decision[i] == CPU) {
            cpu_elapsed_time += cpustats->MaxElapsedTime();
        }
        else if (decision[i] == PIM) {
            pim_elapsed_time += pimstats->MaxElapsedTime();
        }
        else {
            // do nothing, since decision[i] == INVALID means that node i has not
            // been added to the tree
        }
    }
    return std::make_pair(cpu_elapsed_time, pim_elapsed_time);
}

// decision here can be INVALID
COST CostSolver::SwitchCost(const DECISION &decision, const BBLIDBBLSwitchCountList *switchcnt)
{
    COST cur_switch_cost = 0;
    for (auto row : _bbl_switch_count) {
        cur_switch_cost += row.Cost(decision, _switch_cost);
    }
    
    return cur_switch_cost;
}

// decision here should not be INVALID
COST CostSolver::ReuseCost(const DECISION &decision, const BBLIDTrieNode *reusetree)
{
    COST cur_reuse_cost = 0;
    for (auto elem : reusetree->_children) {
        TrieBFS(cur_reuse_cost, decision, elem.first, elem.second, false);
    }
    return cur_reuse_cost;
}

void CostSolver::TrieBFS(COST &cost, const DECISION &decision, BBLID bblid, const BBLIDTrieNode *root, bool isDifferent)
{
    if (root->_isLeaf) {
        // The cost of a segment is zero if and only if the entire segment is in the same place. In other words, if isDifferent, then the cost is non-zero.
        if (isDifferent) {
            // If the initial W is on CPU and there are subsequent R/W on PIM,
            // then this segment contributes to a flush of CPU and data fetch from PIM.
            // We conservatively assume that the fetch will promote data to L1
            assert(bblid == root->_cur);
            if (decision[root->_cur] == CPU) {
                cost += root->_count * (_flush_cost[CPU] + _fetch_cost[PIM]);
            }
            // If the initial W is on PIM and there are subsequent R/W on CPU,
            // then this segment contributes to a flush of PIM and data fetch from CPU
            else {
                cost += root->_count * (_flush_cost[PIM] + _fetch_cost[CPU]);
            }
        }
    }
    else {
        for (auto elem : root->_children) {
            if (isDifferent) {
                TrieBFS(cost, decision, elem.first, elem.second, true);
            }
            else if (decision[bblid] != decision[elem.first]) {
                TrieBFS(cost, decision, elem.first, elem.second, true);
            }
            else {
                TrieBFS(cost, decision, elem.first, elem.second, false);
            }
        }
    }
}