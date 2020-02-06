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

    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < MAX_INDEX; j++) {
            _cost_package->_instruction_latency[i][j] = 1;
        }
    }
}

void InstructionLatency::initialize(CostPackage *cost_package, ConfigReader &reader)
{
    initialize(cost_package);
    ReadConfig(reader);
}

VOID InstructionLatency::InstructionCount(InstructionLatency *self, UINT32 opcode, BOOL ismem, BOOL issimd, THREADID threadid)
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
#ifdef PIMPROFDEBUG
        pkg->_total_instr_cnt++;
        if (issimd) {
            pkg->_total_simd_instr_cnt++;
        }
#endif

        if (bblid == GLOBALBBLID && !pkg->_command_line_parser.enableglobalbbl()) {
            PIN_RWMutexUnlock(&pkg->_thread_count_rwmutex);
            return;
        }

        // theoretical parallelism can only be computed once
        issimd |= pkg->_inAcceleratorFunction;
        issimd &= (!pkg->_bbl_parallelizable[bblid]);

#ifdef PIMPROFDEBUG
        pkg->_bbl_instr_cnt[bblid]++;
        if (issimd) {
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
                if (issimd) {
                    cost = cost / pkg->_core_count[i] * pkg->_simd_cost_multiplier[i];
                }
                pkg->_bbl_instruction_cost[i][bblid] += cost;
#ifdef PIMPROFDEBUG
                if (issimd) {
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
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        for (UINT32 j = 0; j < MAX_INDEX; j++) {
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
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        out << ("[" + CostSiteName[i] + "/InstructionLatency]") << std::endl
            << "; <Instuction Name> = <Instruction Latency>" << std::endl;
        for (UINT32 j = 0; j < MAX_INDEX; j++)
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

VOID MemoryLatency::InstrCacheRef(MemoryLatency *self, ADDRINT addr, UINT32 size, BOOL issimd, THREADID threadid)
{
    CostPackage *pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg->_thread_count_rwmutex);
    BBLID bblid = pkg->_thread_bbl_scope[threadid].top();
    // infomsg() << "instrcache: " << pkg->_thread_count << " " << threadid << std::endl;
    if (!pkg->_thread_in_roi[threadid] && pkg->_roi_decision.empty()) {
        PIN_RWMutexUnlock(&pkg->_thread_count_rwmutex);
        return;
    }
    // pkg->_previous_instr[threadid]++;
    // (*pkg->_trace_file[threadid])
    //     << threadid << " "
    //     << threadid << " " // we assume coreid == threadid
    //     << "- "
    //     << "I "
    //     << addr << " "
    //     << 64 << std::endl;
    if ((pkg->_thread_count == 1 && threadid == 0) ||
    (pkg->_thread_count >= 2 && threadid == 1)) {
        self->_storage->InstrCacheRef(addr, size, bblid, issimd);
    }
    PIN_RWMutexUnlock(&pkg->_thread_count_rwmutex);
}

VOID MemoryLatency::DataCacheRef(MemoryLatency *self, ADDRINT ip, ADDRINT addr, UINT32 size, ACCESS_TYPE accessType, BOOL issimd, THREADID threadid)
{
    CostPackage *pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg->_thread_count_rwmutex);
    BBLID bblid = pkg->_thread_bbl_scope[threadid].top();
    // infomsg() << "datamulti: " << pkg->_thread_count << " " << threadid << std::endl;
    if (!pkg->_thread_in_roi[threadid] && pkg->_roi_decision.empty()) {
        PIN_RWMutexUnlock(&pkg->_thread_count_rwmutex);
        return;
    }
    (*pkg->_trace_file[threadid])
        << "[" << threadid << "] "
        << (accessType == ACCESS_TYPE_LOAD ? "R, " : "W, ")
        << std::hex << "0x" << addr << std::dec << " "
        << size
        << std::hex << " (0x" << ip << ")" << std::dec
        << std::endl;
    pkg->_previous_instr[threadid] = 0;
    if ((pkg->_thread_count == 1 && threadid == 0) ||
    (pkg->_thread_count >= 2 && threadid == 1)) {
        self->_storage->DataCacheRef(addr, size, accessType, bblid, issimd);
    }
    PIN_RWMutexUnlock(&pkg->_thread_count_rwmutex);
}

