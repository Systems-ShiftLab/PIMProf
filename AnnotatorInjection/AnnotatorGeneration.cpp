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
    Function *annotator_head = dyn_cast<Function>(
        M->getOrInsertFunction(
            PIMProfAnnotatorHead, 
            FunctionType::getVoidTy(ctx), 
            Type::getInt32Ty(ctx)
        )
    );
    Function *annotator_tail = dyn_cast<Function>(
        M->getOrInsertFunction(
            PIMProfAnnotatorTail, 
            FunctionType::getVoidTy(ctx), 
            Type::getInt32Ty(ctx)
        )
    );

    // provide definition of function if it has not been defined
    // these functions do nothing so just insert a return instruction
    if (annotator_head->empty()) {
        BasicBlock *temp = BasicBlock::Create(
            ctx, "", annotator_head, 0);
        ReturnInst::Create(ctx, temp);
    }
    if (annotator_tail->empty()) {
        BasicBlock *temp = BasicBlock::Create(
            ctx, "", annotator_tail, 0);
        ReturnInst::Create(ctx, temp);
    }

    std::error_code EC;
    raw_fd_ostream os(PIMProfAnnotatorFileName, EC,
        static_cast<sys::fs::OpenFlags>(0));
    WriteBitcodeToFile(*M, os);

  return 0;
}