//===- PIMProfSolver.cpp - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#include "PIMProfSolver.h"

using namespace PIMProf;

/* ===================================================================== */
/* PIMProfSolver */
/* ===================================================================== */

// Because PIMProfSolver has to be a global variable
// and is dependent on the command line argument,
// we have to use a separate function to initialize it.
void PIMProfSolver::initialize(int argc, char *argv[])
{
// because we assume sizeof(ADDRINT) = 8
# if !(__GNUC__) || !(__x86_64__)
    errormsg() << "Incompatible system" << std::endl;
    assert(0);
#endif
    PIN_InitSymbols();

    _cost_package.initialize(argc, argv);
    _storage.initialize(&_cost_package, _cost_package._config_reader);
    _instruction_latency.initialize(&_cost_package, _cost_package._config_reader);
    _memory_latency.initialize(&_storage, &_cost_package, _cost_package._config_reader);
    .initialize(&_cost_package, _cost_solver_cost_package._config_reader);

}

// void PIMProfSolver::ReadControlFlowGraph(const std::string filename)
// {
//     std::ifstream ifs;
//     ifs.open(filename.c_str());
//     std::string curline;

//     getline(ifs, curline);
//     std::stringstream ss(curline);
//     ss >> _cost_package._bbl_size;
//     _cost_package._bbl_size++; // bbl_size = Largest BBLID + 1
// }


void PIMProfSolver::simulate()
{
    INS_AddInstrumentFunction(InstructionInstrument, (void *)this);
    PIN_AddThreadStartFunction(ThreadStart, (void *)this);
    PIN_AddThreadFiniFunction(ThreadFinish, (void *)this);
    PIN_AddFiniFunction(FinishInstrument, (void *)this);

    // Never returns
    PIN_StartProgram();
}

void PIMProfSolver::HandleMagic(PIMProfSolver *self, ADDRINT bblhash_hi, ADDRINT bblhash_lo, ADDRINT control_value, THREADID threadid)
{
    uint64_t op = ControlValue::GetOpType(control_value);
    uint64_t isomp = ControlValue::GetIsOpenMP(control_value);
    switch(op) {
      case MAGIC_OP_ANNOTATIONHEAD:
        DoAtAnnotationHead(self, bblhash_hi, bblhash_lo, isomp, threadid); break;
      case MAGIC_OP_ANNOTATIONTAIL:
        DoAtAnnotationTail(self, bblhash_hi, bblhash_lo, isomp, threadid); break;
      case MAGIC_OP_ROIBEGIN:
        DoAtROIHead(self, threadid); break;
      case MAGIC_OP_ROIEND:
        DoAtROITail(self, threadid); break;
      case MAGIC_OP_ROIDECISIONBEGIN:
        DoAtROIDecisionHead(self, threadid); break;
      case MAGIC_OP_ROIDECISIONEND:
        DoAtROIDecisionTail(self, threadid); break;
      default:
        errormsg() << "Invalid Control Value " << std::hex << control_value << " " << op << " " << isomp << "." << std::endl;
        assert(0);
    }
}

void PIMProfSolver::DoAtAnnotationHead(PIMProfSolver *self, ADDRINT bblhash_hi, ADDRINT bblhash_lo, ADDRINT isomp, THREADID threadid)
{
    CostPackage &pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg._thread_count_rwmutex);

    auto bblhash = UUID(bblhash_hi, bblhash_lo);
    auto it = pkg._bbl_hash.find(bblhash);
    if (it == pkg._bbl_hash.end()) {
        pkg.initializeNewBBL(bblhash);
        it = pkg._bbl_hash.find(bblhash);
    }
    if (isomp) {
        pkg._in_omp_parallel++;
    }

    pkg._bbl_parallelizable[it->second] |= (pkg._in_omp_parallel > 0);

    // overwrite _bbl_parallelizable[] if in spawned worker thread
    if (threadid == 1) {
        pkg._bbl_parallelizable[it->second] = true;
    }

    if (pkg._command_line_parser.enableroidecision() && pkg._thread_in_roidecision[threadid]) {
        pkg._roi_decision[it->second] = CostSite::PIM;
        pkg._bbl_parallelizable[it->second] = true; // TODO: by default should be commented, uncomment just for test purpose
    }

    pkg._thread_bbl_scope[threadid].push(it->second);

#ifdef PIMPROFTRACE
    (*pkg._trace_file[0]) << "PIMProf BBLStart " << it->second << std::endl;
#endif

#ifdef PIMPROF_MPKI
    pkg._bbl_visit_cnt[it->second]++;
#endif
    // infomsg() << "AnnotationHead: " << pkg._thread_bbl_scope[threadid].top() << " " << it->second << " " << isomp << " " << threadid << " " << pkg._in_omp_parallel << std::endl;

    PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
}

void PIMProfSolver::DoAtAnnotationTail(PIMProfSolver *self, ADDRINT bblhash_hi, ADDRINT bblhash_lo, ADDRINT isomp, THREADID threadid)
{
    CostPackage &pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg._thread_count_rwmutex);

    auto bblhash = UUID(bblhash_hi, bblhash_lo);
    if (isomp) {
        pkg._in_omp_parallel--;
    }
    assert(pkg._thread_bbl_scope[threadid].top() == pkg._bbl_hash[bblhash]);
#ifdef PIMPROFTRACE
    (*pkg._trace_file[0]) << "PIMProf BBLEnd " << pkg._thread_bbl_scope[0].top() << std::endl;
#endif
    // infomsg() << "AnnotationTail: " << pkg._thread_bbl_scope[threadid].top() << " " << pkg._bbl_hash[bblhash] << " " << isomp << " "<< threadid << " " << pkg._in_omp_parallel << std::endl;
    pkg._thread_bbl_scope[threadid].pop();



    PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
}

void PIMProfSolver::DoAtROIHead(PIMProfSolver *self, THREADID threadid)
{
    CostPackage &pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg._thread_count_rwmutex);
    if (pkg._command_line_parser.enableroi()) {
        pkg._thread_in_roi[threadid] = true;
    }
    PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
}

void PIMProfSolver::DoAtROITail(PIMProfSolver *self, THREADID threadid)
{
    CostPackage &pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg._thread_count_rwmutex);
    if (pkg._command_line_parser.enableroi()) {
        pkg._thread_in_roi[threadid] = false;
    }
    PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
}

void PIMProfSolver::DoAtROIDecisionHead(PIMProfSolver *self, THREADID threadid)
{
    CostPackage &pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg._thread_count_rwmutex);
    if (pkg._command_line_parser.enableroidecision()) {
        if (!pkg._thread_in_roidecision[threadid])
            pkg._enter_roi_cnt++;
        pkg._thread_in_roidecision[threadid] = true;
    }
    PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
}

void PIMProfSolver::DoAtROIDecisionTail(PIMProfSolver *self, THREADID threadid)
{
    CostPackage &pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg._thread_count_rwmutex);
    if (pkg._command_line_parser.enableroidecision()) {
        if (pkg._thread_in_roidecision[threadid])
            pkg._exit_roi_cnt++;
        pkg._thread_in_roidecision[threadid] = false;
    }
    PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
}

void PIMProfSolver::DoAtAcceleratorHead(PIMProfSolver *self)
{
    CostPackage &pkg = self->_cost_package;
    pkg._inAcceleratorFunction = true;
    infomsg() << "see EncodeFrame" << std::endl;
}

void PIMProfSolver::DoAtAcceleratorTail(PIMProfSolver *self)
{
    CostPackage &pkg = self->_cost_package;
    pkg._inAcceleratorFunction = false;
}

// void PIMProfSolver::ImageInstrument(IMG img, void *void_self)
// {
//     // find annotator head and tail by their names
//     RTN annotator_head = RTN_FindByName(img, PIMProfAnnotationHead.c_str());
//     RTN annotator_tail = RTN_FindByName(img, PIMProfAnnotationTail.c_str());
//     RTN encode_frame = RTN_FindByName(img, "Encode_frame");

//     if (RTN_Valid(annotator_head) && RTN_Valid(annotator_tail))
//     {
//         // Instrument malloc() to print the input argument value and the return value.
//         RTN_Open(annotator_head);
//         RTN_InsertCall(
//             annotator_head,
//             IPOINT_BEFORE,
//             (AFUNPTR)DoAtAnnotationHead,
//             IARG_PTR, void_self, // Pass the pointer of bbl_scope as an argument of DoAtAnnotationHead
//             IARG_FUNCARG_CALLSITE_VALUE, 0,
//             IARG_FUNCARG_CALLSITE_VALUE, 1,
//             IARG_FUNCARG_CALLSITE_VALUE, 2, // Pass all three function argument PIMProfAnnotationHead as an argument of DoAtAnnotationHead
//             IARG_THREAD_ID,
//             IARG_END);
//         RTN_Close(annotator_head);

//         RTN_Open(annotator_tail);
//         RTN_InsertCall(
//             annotator_tail,
//             IPOINT_BEFORE,
//             (AFUNPTR)DoAtAnnotationTail,
//             IARG_PTR, void_self, // Pass the pointer of bbl_scope as an argument of DoAtAnnotationHead
//             IARG_FUNCARG_CALLSITE_VALUE, 0,
//             IARG_FUNCARG_CALLSITE_VALUE, 1,
//             IARG_FUNCARG_CALLSITE_VALUE, 2, // Pass all three function argument PIMProfAnnotationHead as an argument of DoAtAnnotationTail
//             IARG_THREAD_ID,
//             IARG_END);
//         RTN_Close(annotator_tail);
//     }
//     // Assume that specific function calls can be accelerated
//     // TODO: dirty hack, fix later
//     if (RTN_Valid(encode_frame)) {
//         RTN_Open(encode_frame);
//         RTN_InsertCall(
//             encode_frame,
//             IPOINT_BEFORE,
//             (AFUNPTR)DoAtAcceleratorHead,
//             IARG_PTR, void_self, // Pass the pointer of bbl_scope as an argument of DoAtAnnotationHead
//             IARG_END);
//         RTN_InsertCall(
//             encode_frame,
//             IPOINT_AFTER,
//             (AFUNPTR)DoAtAcceleratorTail,
//             IARG_PTR, void_self, // Pass the pointer of bbl_scope as an argument of DoAtAnnotationHead
//             IARG_END);
//         RTN_Close(encode_frame);
//     }
// }

int ins_to_skip = -1;

void PIMProfSolver::InstructionInstrument(INS ins, void *void_self)
{

    /***** deal with PIMProf magic *****/
    /***
     * Format of magical instructions:
     * 
     * xchg %rcx, %rcx
     * mov <higher bits of the UUID>, %rax
     * mov <lower bits of the UUID>, %rbx
     * mov <the control bits>, %rcx
     * 
     * The magical instructions should all be skipped when analyzing performance
    ***/

    if (INS_IsXchg(ins) && INS_OperandReg(ins, 0) == LEVEL_BASE::REG_RCX && INS_OperandReg(ins, 1) == LEVEL_BASE::REG_RCX) {
        // INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)PrintInfo, IARG_PTR, &std::cout, IARG_PTR, new std::string("is xchg"), IARG_END);
        ins_to_skip = 2;
        return;
    }

    if (ins_to_skip > 0) {
        assert(INS_IsMov(ins));
        // INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)PrintInfo, IARG_PTR, &std::cout, IARG_PTR, new std::string("is mov"), IARG_END);
        ins_to_skip--;
        return;
    }

    if (ins_to_skip == 0) {
        assert(INS_IsMov(ins));
        // INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)PrintInfo, IARG_PTR, &std::cout, IARG_PTR, new std::string("is handlemagic"), IARG_END);
        // instrument the last mov instruction
        INS_InsertCall(
            ins,
            IPOINT_AFTER,
            (AFUNPTR)HandleMagic,
            IARG_PTR, void_self,
            IARG_REG_VALUE, REG_GAX,
            IARG_REG_VALUE, REG_GBX,
            IARG_REG_VALUE, REG_GCX,
            IARG_THREAD_ID,
            IARG_END);
        ins_to_skip = -1;
        return;
    }
/*
    if (INS_IsXchg(ins) && INS_OperandReg(ins, 0) == LEVEL_BASE::REG_RBX && INS_OperandReg(ins, 1) == LEVEL_BASE::REG_RBX) {
        infomsg() << (ins == BBL_InsTail(bbl)) << " " << INS_Valid(ins) << std::endl;
        PrintInstruction(infomsg(), ins);
        for (int i = 0; i < 3; i++) {
            ins = INS_Next(ins);
            infomsg() << (ins == BBL_InsTail(bbl)) << " " << INS_Valid(ins) << std::endl;
            PrintInstruction(infomsg(), ins);
        }
        // instrument the last mov instruction
        INS_InsertCall(
            ins,
            IPOINT_AFTER,
            (AFUNPTR)HandleMagic,
            IARG_PTR, void_self,
            IARG_REG_VALUE, REG_GAX,
            IARG_REG_VALUE, REG_GBX,
            IARG_REG_VALUE, REG_GCX,
            IARG_THREAD_ID,
            IARG_END);
        return;
    }
*/
    /***** deal with non-magical instructions *****/
/*
    if (RTN_Valid(INS_Rtn(ins)))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)PrintInstruction, IARG_PTR, &std::cout, IARG_ADDRINT, INS_Address(ins), IARG_PTR, new std::string(INS_Disassemble(ins) + ", " + RTN_Name(INS_Rtn(ins))), IARG_END);
    else
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)PrintInstruction, IARG_PTR, &std::cout, IARG_ADDRINT, INS_Address(ins), IARG_PTR, new std::string(INS_Disassemble(ins)), IARG_END);
*/
    PIMProfSolver *self = (PIMProfSolver *)void_self;

    uint32_t opcode = (uint32_t)(INS_Opcode(ins));
    bool ismem = INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins);
    xed_decoded_inst_t *xedd = INS_XedDec(ins);

    // simd_len > 0 means that this instruction is parallelizable
    // the value of simd_len indicates the number of normal instructions this simd instruction is equivalent to
    uint32_t simd_len;
    if (!xed_classify_sse(xedd) && !xed_classify_avx(xedd) && !xed_classify_avx512(xedd)) {
        simd_len = 0;
    }
    else if (xed_decoded_inst_get_attribute(xedd, XED_ATTRIBUTE_SIMD_SCALAR)) {
        simd_len = 0;
    }
    else if (xed_classify_sse(xedd)) {
        simd_len = 2;
    }
    else {
        simd_len = xed_decoded_inst_vector_length_bits(xedd) / 64;
    }

    /***** deal with the instruction latency *****/
    INS_InsertCall(
        ins,
        IPOINT_BEFORE,
        (AFUNPTR)InstructionLatency::InstructionCount,
        IARG_PTR, &self->_instruction_latency,
        IARG_ADDRINT, opcode,
        IARG_BOOL, ismem,
        IARG_UINT32, simd_len,
        IARG_THREAD_ID,
        IARG_END);


    uint32_t ins_len = xed_decoded_inst_get_length(xedd);

    // if (simd_len && (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins))) {
    //     INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)PrintInstruction, IARG_PTR, &std::cout, IARG_ADDRINT, INS_Address(ins), IARG_PTR, new std::string(INS_Disassemble(ins)), IARG_UINT32, simd_len, IARG_END);
    // }

    /***** deal with the memory latency *****/
    // all instruction fetches access I-cache
    INS_InsertCall(
        ins,
        IPOINT_BEFORE,
        (AFUNPTR)MemoryLatency::InstrCacheRef,
        IARG_PTR, &self->_memory_latency,
        IARG_INST_PTR,
        IARG_UINT32, ins_len,
        IARG_UINT32, simd_len,
        IARG_THREAD_ID,
        IARG_END);
    if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins)) {
        // only predicated-on memory instructions access D-cache
        INS_InsertPredicatedCall(
            ins,
            IPOINT_BEFORE,
            (AFUNPTR)MemoryLatency::DataCacheRef,
            IARG_PTR, &self->_memory_latency,
            IARG_INST_PTR,
            IARG_MEMORYREAD_EA,
            IARG_MEMORYREAD_SIZE,
            IARG_UINT32, ACCESS_TYPE_LOAD,
            IARG_UINT32, simd_len,
            IARG_THREAD_ID,
            IARG_END);
    }
    if (INS_HasMemoryRead2(ins) && INS_IsStandardMemop(ins)) {
        // only predicated-on memory instructions access D-cache
        INS_InsertPredicatedCall(
            ins,
            IPOINT_BEFORE,
            (AFUNPTR)MemoryLatency::DataCacheRef,
            IARG_PTR, &self->_memory_latency,
            IARG_INST_PTR,
            IARG_MEMORYREAD2_EA,
            IARG_MEMORYREAD_SIZE,
            IARG_UINT32, ACCESS_TYPE_LOAD,
            IARG_UINT32, simd_len,
            IARG_THREAD_ID,
            IARG_END);
    }
    if (INS_IsMemoryWrite(ins) && INS_IsStandardMemop(ins)) {
        // only predicated-on memory instructions access D-cache
        INS_InsertPredicatedCall(
            ins,
            IPOINT_BEFORE,
            (AFUNPTR)MemoryLatency::DataCacheRef,
            IARG_PTR, &self->_memory_latency,
            IARG_INST_PTR,
            IARG_MEMORYWRITE_EA,
            IARG_MEMORYWRITE_SIZE,
            IARG_UINT32, ACCESS_TYPE_STORE,
            IARG_UINT32, simd_len,
            IARG_THREAD_ID,
            IARG_END);
    }
}

#include <memory>

void PIMProfSolver::ThreadStart(THREADID threadid, CONTEXT *ctxt, int32_t flags, void *void_self)
{
    PIMProfSolver *self = (PIMProfSolver *)void_self;
    CostPackage &pkg = self->_cost_package;
    PIN_RWMutexWriteLock(&pkg._thread_count_rwmutex);
    pkg._thread_bbl_scope.push_back(BBLScope());
    if (pkg._command_line_parser.enableroi()) {
        pkg._thread_in_roi.push_back(false);
    }
    else {
        pkg._thread_in_roi.push_back(true);
    }

    if (pkg._command_line_parser.enableroidecision()) {
        pkg._thread_in_roidecision.push_back(false);
    }
    else {
        pkg._thread_in_roidecision.push_back(true);
    }

    pkg._previous_instr.push_back(0);

#ifdef PIMPROFTRACE
    std::ofstream *ofs = new std::ofstream;
    std::stringstream ss;
    ss << pkg._thread_count;
    std::string tracefile = "MemTrace.out." + ss.str();
    ofs->open(tracefile.c_str(), std::ofstream::out);
    pkg._trace_file.push_back(ofs);
#endif

    pkg._thread_count++;
    infomsg() << "ThreadStart:" << threadid << " " << pkg._thread_count << std::endl;
    PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
    // PIN_RWMutexReadLock(&pkg._thread_count_rwmutex);
    // PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
}

void PIMProfSolver::ThreadFinish(THREADID threadid, const CONTEXT *ctxt, int32_t flags, void *void_self)
{
    PIMProfSolver *self = (PIMProfSolver *)void_self;
    CostPackage &pkg = self->_cost_package;
    PIN_RWMutexWriteLock(&pkg._thread_count_rwmutex);
#ifdef PIMPROFTRACE
    for (auto i: pkg._trace_file) {
        i->close();
        delete i;
    }
#endif
    pkg._thread_count--;
    infomsg() << "ThreadEnd:" << threadid << " " << pkg._thread_count << std::endl;
    PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
}

void PIMProfSolver::FinishInstrument(int32_t code, void *void_self)
{
    PIMProfSolver *self = (PIMProfSolver *)void_self;
    CostPackage &pkg = self->_cost_package;
    std::ofstream ofs(
        pkg._command_line_parser.outputfile().c_str(),
        std::ofstream::out);
    DECISION decision = self->_cost_solver.PrintSolution(ofs);
    ofs.close();
    infomsg() << "parallel:" << pkg._in_omp_parallel << std::endl;

    ofs.open("BBLReuseCost.dot", std::ofstream::out);
    pkg._bbl_data_reuse.print(
        ofs,
        pkg._bbl_data_reuse.getRoot());
    ofs.close();

    ofs.open("bblcdf.out", std::ofstream::out);
    // TODO: Need bug fix, cause Pin out of memory error
    self->_cost_solver.PrintAnalytics(ofs);
    self->_instruction_latency.WriteConfig("testconfig.ini");
    ofs.close();

    ofs.open(pkg._command_line_parser.statsfile().c_str(), std::ofstream::out);
    self->_storage.WriteStats(ofs);
    ofs << "Number of times entering ROI: " << pkg._enter_roi_cnt << std::endl;
    ofs << "Number of times exiting ROI: " << pkg._exit_roi_cnt << std::endl;
    self->_cost_solver.PrintAnalytics(ofs);
    ofs.close();
}
