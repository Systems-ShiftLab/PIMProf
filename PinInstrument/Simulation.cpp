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

    // SetBBLSize(_cost_package->_bbl_size);
}

void InstructionLatency::initialize(CostPackage *cost_package, ConfigReader &reader)
{
    initialize(cost_package);
    ReadConfig(reader);
}

// void InstructionLatency::SetBBLSize(BBLID bbl_size) {
//     for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
//         _cost_package->_bbl_instruction_cost[i].resize(bbl_size);
//         memset(&_cost_package->_bbl_instruction_cost[i][0], 0, bbl_size * sizeof _cost_package->_bbl_instruction_cost[i][0]);
//     }
// }


void InstructionLatency::instrument() {
    INS_AddInstrumentFunction(InstructionInstrument, (VOID *)this);
}


VOID InstructionLatency::InstructionCount(InstructionLatency *self, UINT32 opcode, BOOL ismem, BOOL issimd, THREADID threadid)
{
    PIN_RWMutexReadLock(&self->_cost_package->_thread_count_rwmutex);
    BBLID bblid = self->_cost_package->_thread_bbl_scope[threadid].top();
    // infomsg() << "instrcount: " << self->_cost_package->_thread_count << " " << threadid << std::endl;
    if ((self->_cost_package->_thread_count == 1 && threadid == 0) ||
    (self->_cost_package->_thread_count >= 2 && threadid == 1)) {
#ifdef PIMPROFDEBUG
        self->_cost_package->_total_instr_cnt++;
        if (issimd) {
            self->_cost_package->_total_simd_instr_cnt++;
        }
#endif

        if (bblid == GLOBALBBLID) {
            PIN_RWMutexUnlock(&self->_cost_package->_thread_count_rwmutex);
            return;
        }
        // theoretical parallelism can only be computed once
        issimd |= self->_cost_package->_inAcceleratorFunction;
        issimd &= (!self->_cost_package->_inParallelRegion[bblid]);

#ifdef PIMPROFDEBUG
        self->_cost_package->_bbl_instr_cnt[bblid]++;
        if (issimd) {
            self->_cost_package->_simd_instr_cnt[bblid]++;
        }
        self->_cost_package->_type_instr_cnt[opcode]++;
#endif

        if (ismem) { /* do nothing*/ }
        else {
            for (int i = 0; i < MAX_COST_SITE; i++) {
                // estimating the cost of all threads by looking at the cost of only one thread
                COST cost = self->_cost_package->_instruction_latency[i][opcode] * self->_cost_package->_thread_count;
                // if we assume a SIMD instruction can be infinitely parallelized,
                // then the cost of each instruction will be 1/n if we are having n cores.
                if (issimd) {
                    cost = cost / self->_cost_package->_core_count[i] * self->_cost_package->_simd_cost_multiplier[i];
                }
                self->_cost_package->_bbl_instruction_cost[i][bblid] += cost;
#ifdef PIMPROFDEBUG
                if (issimd) {
                    self->_cost_package->_total_simd_cost[i] += cost;
                }
                self->_cost_package->_type_instr_cost[i][opcode] += cost;
#endif
            }
        }
    }
    PIN_RWMutexUnlock(&self->_cost_package->_thread_count_rwmutex);
}

VOID InstructionLatency::InstructionInstrument(INS ins, VOID *void_self)
{
    RTN rtn = INS_Rtn(ins);
    std::string rtn_name = "";
    if (RTN_Valid(rtn))
        rtn_name = RTN_Name(rtn);
    // do not instrument the annotation function
    // regions with invalid names can be JIT code, for example, so cannot be ignored
    if (rtn_name != PIMProfAnnotationHead && rtn_name != PIMProfAnnotationTail) {
        InstructionLatency *self = (InstructionLatency *)void_self;
        UINT32 opcode = (UINT32)(INS_Opcode(ins));
        BOOL ismem = INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins);
        xed_decoded_inst_t *xedd = INS_XedDec(ins);
        BOOL issimd = xed_decoded_inst_get_attribute(xedd, XED_ATTRIBUTE_SIMD_SCALAR);
        // infomsg() << std::hex << INS_Address(ins) << " " << OPCODE_StringShort(opcode) << std::endl;
        // TODO: fix it later
        issimd |= (OPCODE_StringShort(opcode)[0] == 'V');
        if (issimd) {
            infomsg() << std::hex << INS_Address(ins) << std::dec << " " << OPCODE_StringShort(opcode) << std::endl;
        }
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)InstructionCount,
            IARG_PTR, (VOID *)self,
            IARG_ADDRINT, opcode,
            IARG_BOOL, ismem,
            IARG_BOOL, issimd,
            IARG_THREAD_ID,
            IARG_END);
    }
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


void MemoryLatency::instrument()
{
    INS_AddInstrumentFunction(InstructionInstrument, (VOID *)this);
    PIN_AddFiniFunction(FinishInstrument, (VOID *)this);
}


// VOID MemoryLatency::SetBBLSize(BBLID bbl_size) {
//     for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
//         _cost_package->_bbl_memory_cost[i].resize(bbl_size);
//         memset(&_cost_package->_bbl_memory_cost[i][0], 0, bbl_size * sizeof _cost_package->_bbl_memory_cost[i][0]);
//     }
// }


VOID MemoryLatency::InstrCacheRef(MemoryLatency *self, ADDRINT addr, BOOL issimd, THREADID threadid)
{
    PIN_RWMutexReadLock(&self->_cost_package->_thread_count_rwmutex);
    BBLID bblid = self->_cost_package->_thread_bbl_scope[threadid].top();
    // infomsg() << "instrcache: " << self->_cost_package->_thread_count << " " << threadid << std::endl;
    if ((self->_cost_package->_thread_count == 1 && threadid == 0) ||
    (self->_cost_package->_thread_count >= 2 && threadid == 1)) {
        self->_storage->InstrCacheRef(addr, bblid, issimd);
    }
    PIN_RWMutexUnlock(&self->_cost_package->_thread_count_rwmutex);
    // if (PIN_RWMutexTryWriteLock(&self->_cost_package->_thread_count_rwmutex)) {
    //     infomsg() << "inst" << std::endl;
    //     PIN_RWMutexUnlock(&self->_cost_package->_thread_count_rwmutex);
    // }
    // else {
    //     infomsg() << "fck inst" << std::endl;
    // }
}

VOID MemoryLatency::DataCacheRefMulti(MemoryLatency *self, ADDRINT addr, UINT32 size, ACCESS_TYPE accessType, BOOL issimd, THREADID threadid)
{
    PIN_RWMutexReadLock(&self->_cost_package->_thread_count_rwmutex);
    BBLID bblid = self->_cost_package->_thread_bbl_scope[threadid].top();
    // infomsg() << "datamulti: " << self->_cost_package->_thread_count << " " << threadid << std::endl;
    if ((self->_cost_package->_thread_count == 1 && threadid == 0) ||
    (self->_cost_package->_thread_count >= 2 && threadid == 1)) {
        self->_storage->DataCacheRefMulti(addr, size, accessType, bblid, issimd);
    }
    PIN_RWMutexUnlock(&self->_cost_package->_thread_count_rwmutex);
    // if (PIN_RWMutexTryWriteLock(&self->_cost_package->_thread_count_rwmutex)) {
    //     infomsg() << "datamulti" << std::endl;
    //     PIN_RWMutexUnlock(&self->_cost_package->_thread_count_rwmutex);
    // }
    // else {
    //     infomsg() << "fck datamulti" << std::endl;
    // }
}

VOID MemoryLatency::DataCacheRefSingle(MemoryLatency *self, ADDRINT addr, UINT32 size, ACCESS_TYPE accessType, BOOL issimd, THREADID threadid)
{
    PIN_RWMutexReadLock(&self->_cost_package->_thread_count_rwmutex);
    BBLID bblid = self->_cost_package->_thread_bbl_scope[threadid].top();
    // infomsg() << "datasingle: " << self->_cost_package->_thread_count << " " << threadid << std::endl;
    if ((self->_cost_package->_thread_count == 1 && threadid == 0) ||
    (self->_cost_package->_thread_count >= 2 && threadid == 1)) {
        self->_storage->DataCacheRefSingle(addr, size, accessType, bblid, issimd);
    }
    PIN_RWMutexUnlock(&self->_cost_package->_thread_count_rwmutex);
    // if (PIN_RWMutexTryWriteLock(&self->_cost_package->_thread_count_rwmutex)) {
    //     infomsg() << "datasingle" << std::endl;
    //     PIN_RWMutexUnlock(&self->_cost_package->_thread_count_rwmutex);
    // }
    // else {
    //     infomsg() << "fck datasingle" << std::endl;
    // }
}

VOID MemoryLatency::InstructionInstrument(INS ins, VOID *void_self)
{
    RTN rtn = INS_Rtn(ins);
    std::string rtn_name = "";
    if (RTN_Valid(rtn))
        rtn_name = RTN_Name(rtn);
    // do not instrument the annotation function
    if (rtn_name != PIMProfAnnotationHead && rtn_name != PIMProfAnnotationTail) {
        MemoryLatency *self = (MemoryLatency *)void_self;
        xed_decoded_inst_t *xedd = INS_XedDec(ins);
        BOOL issimd = xed_decoded_inst_get_attribute(xedd, XED_ATTRIBUTE_SIMD_SCALAR);
        // all instruction fetches access I-cache
        INS_InsertCall(
            ins, IPOINT_BEFORE, (AFUNPTR)InstrCacheRef,
            IARG_PTR, (VOID *)self,
            IARG_INST_PTR,
            IARG_BOOL, issimd,
            IARG_THREAD_ID,
            IARG_END);
        if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins)) {
            const UINT32 size = INS_MemoryReadSize(ins);
            const AFUNPTR DataCacheRef = (size <= 4 ? (AFUNPTR)DataCacheRefSingle : (AFUNPTR)DataCacheRefMulti);

            // only predicated-on memory instructions access D-cache
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, DataCacheRef,
                IARG_PTR, (VOID *)self,
                IARG_MEMORYREAD_EA,
                IARG_MEMORYREAD_SIZE,
                IARG_UINT32, ACCESS_TYPE_LOAD,
                IARG_BOOL, issimd,
                IARG_THREAD_ID,
                IARG_END);
        }
        if (INS_IsMemoryWrite(ins) && INS_IsStandardMemop(ins)) {
            const UINT32 size = INS_MemoryWriteSize(ins);
            const AFUNPTR DataCacheRef = (size <= 4 ? (AFUNPTR)DataCacheRefSingle : (AFUNPTR)DataCacheRefMulti);

            // only predicated-on memory instructions access D-cache
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, DataCacheRef,
                IARG_PTR, (VOID *)self,
                IARG_MEMORYWRITE_EA,
                IARG_MEMORYWRITE_SIZE,
                IARG_UINT32, ACCESS_TYPE_STORE,
                IARG_BOOL, issimd,
                IARG_THREAD_ID,
                IARG_END);
        }
    }
}

VOID MemoryLatency::FinishInstrument(INT32 code, VOID *void_self)
{
    // nothing to do
}
