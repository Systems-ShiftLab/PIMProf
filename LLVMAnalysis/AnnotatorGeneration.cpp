//===- AnnotatorGeneration.cpp - Generate BB annotator ----------*- C++ -*-===//
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

#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"

#include "Common.h"

using namespace llvm;

LLVMContext ctx;

// provide definition of function if it has not been defined
// intended behavior:
// ; Function Attrs: noinline nounwind optnone uwtable
// define i64 @Annotator(i64, i64, i64) {
//     %2 = alloca i64, align 4
//     store i64 %0, i64* %2, align 4
//     %3 = load i64, i64* %2, align 4
//     ret i64 %3
// }
void CreateAnnotatorFunction(const std::string name, Module &M)
{
    // declare annotator function 
    Function *annotator = dyn_cast<Function>(
        M.getOrInsertFunction(
            name, 
            FunctionType::getInt64Ty(ctx), 
            Type::getInt64Ty(ctx),
            Type::getInt64Ty(ctx),
            Type::getInt64Ty(ctx)
        )
    );

    // add attribute to function
    // which suppresses essentially all optimizations on a function or method
    annotator->addFnAttr(Attribute::NoInline);
    annotator->addFnAttr(Attribute::NoUnwind);
    annotator->addFnAttr(Attribute::OptimizeNone);
    annotator->addFnAttr(Attribute::UWTable);

    // create instructions
    if (annotator->empty()) {
        BasicBlock *temp = BasicBlock::Create(
            ctx, "", annotator, 0);
        auto al = new AllocaInst(Type::getInt64Ty(ctx), 0, "", temp);
        al->setAlignment(4);

        auto it = annotator->arg_begin();
        auto eit = annotator->arg_end();
        for (; it != eit; it++) {
            auto st = new StoreInst(it, al, temp);
            st->setAlignment(4);
        }
        
        auto ld = new LoadInst(al, "", temp);
        ld->setAlignment(4);
        auto rt = ReturnInst::Create(ctx, ld, temp);
        // insert instruction metadata
        MDNode* md = MDNode::get(
            ctx, 
            ConstantAsMetadata::get(
                ConstantInt::get(
                    IntegerType::get(M.getContext(), 64), PIMProfAnnotatorBBLID)
            )
        );
        rt->setMetadata(PIMProfBBLIDMetadata, md);
    }
}


int main(int argc, char **argv) {
    // Module Construction
    Module *M = new Module("AnnotatorGeneration", ctx);
    M->setSourceFileName("AnnotatorGeneration");
    M->setDataLayout("e-m:e-i64:64-f80:128-n8:16:32:64-S128");

    CreateAnnotatorFunction(PIMProfAnnotatorHead, *M);
    CreateAnnotatorFunction(PIMProfAnnotatorTail, *M);

    std::error_code EC;
    raw_fd_ostream os(PIMProfAnnotatorFileName, EC,
        static_cast<sys::fs::OpenFlags>(0));
    WriteBitcodeToFile(*M, os);

  return 0;
}
