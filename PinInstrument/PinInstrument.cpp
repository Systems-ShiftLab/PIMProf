//===- PinInstrument.cpp - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#include "PinInstrument.h"

using namespace PIMProf;

/* ===================================================================== */
/* PinInstrument */
/* ===================================================================== */

// Because PinInstrument has to be a global variable
// and is dependent on the command line argument,
// we have to use a separate function to initialize it.
void PinInstrument::initialize(int argc, char *argv[])
{
// because we assume sizeof(ADDRINT) = 8
# if !(__GNUC__) || !(__x86_64__)
    errormsg() << "Incompatible system" << std::endl;
    ASSERTX(0);
#endif
    PIN_InitSymbols();

    _command_line_parser.initialize(argc, argv);
    
    // ReadControlFlowGraph(_command_line_parser.controlflowfile());

    _config_reader = ConfigReader(_command_line_parser.configfile());
    _cost_package.initialize();
    _storage.initialize(&_cost_package, _config_reader);
    _instruction_latency.initialize(&_cost_package, _config_reader);
    _memory_latency.initialize(&_storage, &_cost_package, _config_reader);
    _cost_solver.initialize(&_cost_package, _config_reader);

}

// void PinInstrument::ReadControlFlowGraph(const std::string filename)
// {
//     std::ifstream ifs;
//     ifs.open(filename.c_str());
//     std::string curline;

//     getline(ifs, curline);
//     std::stringstream ss(curline);
//     ss >> _cost_package._bbl_size;
//     _cost_package._bbl_size++; // bbl_size = Largest BBLID + 1
// }


void PinInstrument::instrument()
{
    IMG_AddInstrumentFunction(ImageInstrument, (VOID *)this);
    PIN_AddFiniFunction(PinInstrument::FinishInstrument, (VOID *)this);
}

void PinInstrument::simulate()
{
    instrument();
    _instruction_latency.instrument();
    _memory_latency.instrument();

    // Never returns
    PIN_StartProgram();
}

VOID PinInstrument::DoAtAnnotationHead(PinInstrument *self, ADDRINT bblhash_hi, ADDRINT bblhash_lo, ADDRINT isomp)
{
    CostPackage &pkg = self->_cost_package;
    // infomsg() << "AnnotationHead: " << std::hex << bblhash_hi << " " << bblhash_lo << " " << isomp << std::endl;
    auto bblhash = UUID(bblhash_hi, bblhash_lo);
    auto it = pkg._bbl_hash.find(bblhash);
    if (it == pkg._bbl_hash.end()) {
        pkg._bbl_hash[bblhash] = pkg._bbl_size;
        it = pkg._bbl_hash.find(bblhash);
        pkg._bbl_size++;
        pkg._inOpenMPRegion.push_back(isomp);
        for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
            pkg._bbl_instruction_cost[i].push_back(0);
        }
        for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
            pkg._bbl_memory_cost[i].push_back(0);
        }
        pkg._bbl_visit_cnt.push_back(0);
        pkg._instr_cnt.push_back(0);
    }
    pkg._bbl_scope.push(it->second);
    self->_cost_package._bbl_visit_cnt[it->second]++;

    // infomsg() << it->second << " " << isomp << std::endl;
}

VOID PinInstrument::DoAtAnnotationTail(PinInstrument *self, ADDRINT bblhash_hi, ADDRINT bblhash_lo, ADDRINT isomp)
{
    CostPackage &pkg = self->_cost_package;
    // infomsg() << "AnnotationTail: " << std::hex << bblhash_hi << " " << bblhash_lo << " " << isomp << std::endl;
    auto bblhash = UUID(bblhash_hi, bblhash_lo);
    ASSERTX(pkg._bbl_scope.top() == pkg._bbl_hash[bblhash]);
    pkg._bbl_scope.pop();
}

VOID PinInstrument::ImageInstrument(IMG img, VOID *void_self)
{
    // find annotator head and tail by their names
    RTN annotator_head = RTN_FindByName(img, PIMProfAnnotationHead.c_str());
    RTN annotator_tail = RTN_FindByName(img, PIMProfAnnotationTail.c_str());

    if (RTN_Valid(annotator_head) && RTN_Valid(annotator_tail))
    {
        // Instrument malloc() to print the input argument value and the return value.
        RTN_Open(annotator_head);
        RTN_InsertCall(
            annotator_head,
            IPOINT_BEFORE,
            (AFUNPTR)DoAtAnnotationHead,
            IARG_PTR, void_self, // Pass the pointer of bbl_scope as an argument of DoAtAnnotationHead
            IARG_FUNCARG_CALLSITE_VALUE, 0,
            IARG_FUNCARG_CALLSITE_VALUE, 1,
            IARG_FUNCARG_CALLSITE_VALUE, 2, // Pass all three function argument PIMProfAnnotationHead as an argument of DoAtAnnotationHead
            IARG_END);
        RTN_Close(annotator_head);

        RTN_Open(annotator_tail);
        RTN_InsertCall(
            annotator_tail,
            IPOINT_BEFORE,
            (AFUNPTR)DoAtAnnotationTail,
            IARG_PTR, void_self, // Pass the pointer of bbl_scope as an argument of DoAtAnnotationHead
            IARG_FUNCARG_CALLSITE_VALUE, 0,
            IARG_FUNCARG_CALLSITE_VALUE, 1,
            IARG_FUNCARG_CALLSITE_VALUE, 2, // Pass all three function argument PIMProfAnnotationHead as an argument of DoAtAnnotationTail
            IARG_END);
        RTN_Close(annotator_tail);
    }
}

VOID PinInstrument::FinishInstrument(INT32 code, VOID *void_self)
{
    PinInstrument *self = (PinInstrument *)void_self;
    std::ofstream ofs(self->_command_line_parser.outputfile().c_str(), std::ofstream::out);
    CostSolver::DECISION decision = self->_cost_solver.PrintSolution(ofs);
    ofs.close();
    
    ofs.open("BBLReuseCost.dot", std::ofstream::out);
    self->_cost_package._data_reuse.print(ofs, self->_cost_package._data_reuse.getRoot());
    ofs.close();

    ofs.open("bblcdf.out", std::ofstream::out);
    self->_cost_solver.PrintAnalytics(ofs);
}
