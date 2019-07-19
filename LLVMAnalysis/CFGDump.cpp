//===- CFGDump.cpp - Read bitcode file and process CFG ----------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/IRPrintingPasses.h"

#include "llvm/IRReader/IRReader.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/CommandLine.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include "assert.h"

#include "CFGDump.h"
#include "Common.h"

using namespace llvm;

LLVMContext ctx;

static cl::opt<std::string> InputFilename(
    cl::Positional,
    cl::desc("<input file>"),
    cl::Required
);

static cl::opt<std::string> OutputFilename(
    "o",
    cl::desc("Specify output filename for dumping basic block information."),
    cl::value_desc("output file"),
    cl::init("basicblock.dump")
);

void BasicBlockDFS(BasicBlock *BB, int depth) {
    std::string blanks(depth * 2, ' ');
    errs() << blanks << depth << BB->getName() << "\n";
    const TerminatorInst *TInst = BB->getTerminator();
    for (unsigned I = 0, NSucc = TInst->getNumSuccessors(); I < NSucc; ++I) {
        BasicBlock *Succ = TInst->getSuccessor(I);
        BasicBlockDFS(Succ, depth + 1);
    // Do stuff with Succ
    }
}

void PIMProf::CFGDump() {
    // Module Construction
    std::unique_ptr<Module> M;
    
    SMDiagnostic error;
    M = parseIRFile(InputFilename, error, ctx);
    if (M == nullptr) {
        errs() << "CFGDump: Input filename read error.\n";
        assert(false);
    }

    std::stringstream cfgss;
    int maxbblid = 0;

    // dump the entire CFG of the module
    for (auto &func : *M) {
        for (auto &bb: func) {
            TerminatorInst *headT = bb.getTerminator();
            ConstantAsMetadata *head = dyn_cast<ConstantAsMetadata>(
                dyn_cast<MDNode>(
                    headT->getMetadata(PIMProfBBLIDMetadata))->getOperand(0)
            );
            APInt headAPVal = dyn_cast<ConstantInt>(
                head->getValue())->getValue();
            // get integer value of APInt
            int headVal = (int) headAPVal.getLimitedValue(UINT32_MAX);

            // skip the annotator function
            if (headVal == PIMProfAnnotatorBBLID)
                continue;
            
            if (headVal > maxbblid)
                maxbblid = headVal;

            cfgss << headVal << " ";

            for (unsigned i = 0, n = headT->getNumSuccessors(); i < n; i++) {
                TerminatorInst *tailT = headT->getSuccessor(i)->getTerminator();
                ConstantAsMetadata *tail = dyn_cast<ConstantAsMetadata>(
                    dyn_cast<MDNode>(
                        tailT->getMetadata(PIMProfBBLIDMetadata))->getOperand(0)
                );
                APInt tailAPVal = dyn_cast<ConstantInt>(
                    tail->getValue())->getValue();
                int tailVal = (int) tailAPVal.getLimitedValue(UINT32_MAX);
                cfgss << tailVal << " ";
              // Do stuff with Succ
            }
            cfgss << std::endl;
        }
    }

    std::ofstream ofs;
    ofs.open(OutputFilename);
    ofs << maxbblid << std::endl;
    ofs << cfgss.rdbuf();
    ofs.close();
}

int main(int argc, char **argv) {
    cl::ParseCommandLineOptions(argc, argv);
    PIMProf::CFGDump();
    return 0;
}
