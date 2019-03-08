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

#include "CFGDump.h"
#include "Common.h"

using namespace llvm;

LLVMContext ctx;

int main(int argc, char **argv) {
    // Module Construction
    std::unique_ptr<Module> M;
    if (argc != 2) {
        errs()  << "Incorrect input format.\n"
                << "Usage ./CFGDump.exe <filename>\n";
        return -1;
    }
    else {
        SMDiagnostic error;
        M = parseIRFile(argv[1], error, ctx);
        if (M == nullptr) {
            errs()  << "Filename error.\n"
                    << "Usage ./CFGDump.exe <filename>\n";
            return -1;
        }
    }

    

    // graph
    M->print(errs(), nullptr);
  return 0;
}