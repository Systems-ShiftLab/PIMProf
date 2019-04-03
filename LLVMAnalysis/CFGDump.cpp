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

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"

#include <fstream>

#include "CFGDump.h"
#include "Common.h"

using namespace llvm;

LLVMContext ctx;

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

int PIMProf::CFGDump(const std::string &input, const std::string &output) {
    // Module Construction
    std::unique_ptr<Module> M;
    
    SMDiagnostic error;
    M = parseIRFile(input, error, ctx);
    if (M == nullptr) {
        errs() << "CFGDump: Input filename read error.\n";
        return -1;
    }

    std::ofstream ofs;
    ofs.open(output);

    // dump the entire CFG of the module
    for (auto &func : *M) {
        for (auto &bb: func) {
            TerminatorInst *headT = bb.getTerminator();
            ConstantAsMetadata *head = dyn_cast<ConstantAsMetadata>(
                dyn_cast<MDNode>(
                    headT->getMetadata(PIMProfBBIDMetadata))->getOperand(0)
            );
            APInt headAPVal = dyn_cast<ConstantInt>(
                head->getValue())->getValue();
            // get integer value of APInt
            int headVal = (int) headAPVal.getLimitedValue(UINT32_MAX);

            // skip the annotator function
            if (headVal == PIMProfAnnotatorBBID)
                continue;

            ofs << headVal << " ";

            for (unsigned i = 0, n = headT->getNumSuccessors(); i < n; i++) {
                TerminatorInst *tailT = headT->getSuccessor(i)->getTerminator();
                ConstantAsMetadata *tail = dyn_cast<ConstantAsMetadata>(
                    dyn_cast<MDNode>(
                        tailT->getMetadata(PIMProfBBIDMetadata))->getOperand(0)
                );
                APInt tailAPVal = dyn_cast<ConstantInt>(
                    tail->getValue())->getValue();
                int tailVal = (int) tailAPVal.getLimitedValue(UINT32_MAX);
                ofs << tailVal << " ";
              // Do stuff with Succ
            }
            ofs << "\n";
        }
    }

    ofs.close();
}

int main(int argc, char **argv) {
    if (argc != 3) {
        errs()  << "Incorrect number of arguments provided.\n"
                << "Usage ./CFGDump.exe <inputfile> <outputfile>\n";
        return -1;
    }
    else {
        PIMProf::CFGDump(argv[1], argv[2]);
    }
    
    return 0;
}
