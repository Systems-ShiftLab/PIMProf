#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/IRPrintingPasses.h"

#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"

#include "Common.h"
#include "fstream"

using namespace llvm;

LLVMContext ctx;


int main(int argc, char **argv) {
    // Module Construction
    Module *M = new Module("AnnotatorGeneration", ctx);

    // declare annotator function 
    Function *annotator = dyn_cast<Function>(
        M->getOrInsertFunction(
            annotatorName, 
            FunctionType::getVoidTy(ctx), 
            Type::getInt32Ty(ctx)
        )
    );

    // provide definition of function if it has not been defined 
    if (annotator->empty()) {
        BasicBlock *temp = BasicBlock::Create(
            ctx, "", annotator, 0);
        ReturnInst::Create(ctx, temp);
    }

    std::error_code EC;
    raw_fd_ostream os(annotatorFileName, EC, static_cast<sys::fs::OpenFlags>(0));
    WriteBitcodeToFile(*M, os);

  return 0;
}