//===- InstructionLatency.cpp - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "Simulation.h"

using namespace PIMProf;

/* ===================================================================== */
/* InstructionLatency */
/* ===================================================================== */


void InstructionLatency::initialize(CostPackage *cost_package)
{
    _cost_package = cost_package;

    for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
        for (uint32_t j = 0; j < MAX_INDEX; j++) {
            _cost_package->_instruction_latency[i][j] = 1;
        }
    }
}

void InstructionLatency::initialize(CostPackage *cost_package, ConfigReader &reader)
{
    initialize(cost_package);
    ReadConfig(reader);
}

void InstructionLatency::InstructionCount(InstructionLatency *self, uint32_t opcode, bool ismem, uint32_t simd_len, THREADID threadid)
{
    CostPackage *pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg->_thread_count_rwmutex);
    BBLID bblid = pkg->_thread_bbl_scope[threadid].top();
    // infomsg() << "instrcount: " << pkg->_thread_count << " " << threadid << std::endl;
    if (!pkg->_thread_in_roi[threadid] && pkg->_roi_decision.empty()) {
        PIN_RWMutexUnlock(&pkg->_thread_count_rwmutex);
        return;
    }
    if ((pkg->_thread_count == 1 && threadid == 0) ||
    (pkg->_thread_count >= 2 && threadid == 1)) {
#ifdef PIMPROF_MPKI
        pkg->_total_instr_cnt++;
        if (simd_len) {
            pkg->_total_simd_instr_cnt++;
        }
#endif

        if (bblid == GLOBALBBLID && !pkg->_command_line_parser.enableglobalbbl()) {
            PIN_RWMutexUnlock(&pkg->_thread_count_rwmutex);
            return;
        }

        // if this instruction itself is not a simd instruction
        // but the instruction is in an accelerable function or parallelizable region,
        // then this instruction is a normal instruction but parallelizable (simd_len = 1)
        if (simd_len == 0 && (pkg->_inAcceleratorFunction || pkg->_bbl_parallelizable[bblid])) {
            simd_len = 1;
        }


#ifdef PIMPROF_MPKI
        pkg->_bbl_instr_cnt[bblid]++;
        if (simd_len) {
            pkg->_simd_instr_cnt[bblid]++;
        }
        pkg->_type_instr_cnt[opcode]++;
#endif

        if (ismem) { /* do nothing*/ }
        else {
            for (int i = 0; i < MAX_COST_SITE; i++) {
                // estimating the cost of all threads by looking at the cost of only one thread
                COST cost = pkg->_instruction_latency[i][opcode] * pkg->_thread_count;
                // if we assume a SIMD instruction can be infinitely parallelized,
                // then the cost of each instruction will be 1/n if we are having n cores.
                if (simd_len) {
                    int multiplier = (pkg->_simd_capability[i]<simd_len ? simd_len/pkg->_simd_capability[i] : 1);
                    cost = cost * multiplier / pkg->_core_count[i];
                }
                pkg->_bbl_instruction_cost[i][bblid] += cost;
#ifdef PIMPROF_MPKI
                if (simd_len) {
                    pkg->_total_simd_cost[i] += cost;
                }
                pkg->_type_instr_cost[i][opcode] += cost;
#endif
            }
        }
        // std::cout << OPCODE_StringShort(opcode) << std::endl;
    }
    PIN_RWMutexUnlock(&pkg->_thread_count_rwmutex);
}

void InstructionLatency::ReadConfig(ConfigReader &reader)
{
    for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
        for (uint32_t j = 0; j < MAX_INDEX; j++) {
            std::string opcodestr = OPCODE_StringShort(j);
            if (opcodestr != "LAST") {
                COST latency = reader.GetReal(CostSiteName[i] + "/InstructionLatency", opcodestr, -1);
                if (latency >= 0) {
                    _cost_package->_instruction_latency[i][j] = latency;
                }
                else {
                    _cost_package->_instruction_latency[i][j] = 1;
                }
            }
        }
    }
}

std::ostream& InstructionLatency::WriteConfig(std::ostream& out)
{
    for (uint32_t i = 0; i < MAX_COST_SITE; i++) {
        out << ("[" + CostSiteName[i] + "/InstructionLatency]") << std::endl
            << "; <Instuction Name> = <Instruction Latency>" << std::endl;
        for (uint32_t j = 0; j < MAX_INDEX; j++)
        {
            std::string opcodestr = OPCODE_StringShort(j);
            if (opcodestr != "LAST") {
                opcodestr = ljstr(opcodestr, 15);
                out << opcodestr << "= " << _cost_package->_instruction_latency[i][j] << std::endl;
            }
        }
        out << std::endl;
    }
    return out;
}

void InstructionLatency::WriteConfig(const std::string filename)
{
    std::ofstream out;
    out.open(filename.c_str(), std::ios_base::out);
    WriteConfig(out);
    out.close();
}

/* ===================================================================== */
/* MemoryLatency */
/* ===================================================================== */

void MemoryLatency::initialize(STORAGE *storage, CostPackage *cost_package, ConfigReader &reader)
{
    _storage = storage;
    _cost_package = cost_package;
    // SetBBLSize(_cost_package->_bbl_size);
}

void MemoryLatency::InstrCacheRef(MemoryLatency *self, ADDRINT addr, uint32_t size, uint32_t simd_len, THREADID threadid)
{
    CostPackage *pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg->_thread_count_rwmutex);
    BBLID bblid = pkg->_thread_bbl_scope[threadid].top();
    // infomsg() << "instrcache: " << pkg->_thread_count << " " << threadid << std::endl;
    if (!pkg->_thread_in_roi[threadid] && pkg->_roi_decision.empty()) {
        PIN_RWMutexUnlock(&pkg->_thread_count_rwmutex);
        return;
    }
    pkg->_previous_instr[threadid]++;
    if ((pkg->_thread_count == 1 && threadid == 0) ||
    (pkg->_thread_count >= 2 && threadid == 1)) {
        self->_storage->InstrCacheRef(addr, size, bblid, simd_len);
    }
    PIN_RWMutexUnlock(&pkg->_thread_count_rwmutex);
}

void MemoryLatency::DataCacheRef(MemoryLatency *self, ADDRINT ip, ADDRINT addr, uint32_t size, ACCESS_TYPE accessType, uint32_t simd_len, THREADID threadid)
{
    CostPackage *pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg->_thread_count_rwmutex);
    BBLID bblid = pkg->_thread_bbl_scope[threadid].top();
    // infomsg() << "datamulti: " << pkg->_thread_count << " " << threadid << std::endl;
    if (!pkg->_thread_in_roi[threadid] && pkg->_roi_decision.empty()) {
        PIN_RWMutexUnlock(&pkg->_thread_count_rwmutex);
        return;
    }
    pkg->_previous_instr[threadid] = 0;
    if ((pkg->_thread_count == 1 && threadid == 0) ||
    (pkg->_thread_count >= 2 && threadid == 1)) {
        self->_storage->DataCacheRef(ip, addr, size, accessType, bblid, simd_len);
    }
    PIN_RWMutexUnlock(&pkg->_thread_count_rwmutex);
}

