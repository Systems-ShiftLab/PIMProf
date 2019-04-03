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
// define i32 @Annotator(i32) {
//     %2 = alloca i32, align 4
//     store i32 %0, i32* %2, align 4
//     %3 = load i32, i32* %2, align 4
//     ret i32 %3
// }
void CreateAnnotatorFunction(const std::string name, Module &M)
{
    // declare annotator function 
    Function *annotator = dyn_cast<Function>(
        M.getOrInsertFunction(
            name, 
            FunctionType::getInt32Ty(ctx), 
            Type::getInt32Ty(ctx)
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
        auto al = new AllocaInst(Type::getInt32Ty(ctx), 0, "", temp);
        al->setAlignment(4);
        auto st = new StoreInst(annotator->arg_begin(), al, temp);
        st->setAlignment(4);
        auto ld = new LoadInst(al, "", temp);
        ld->setAlignment(4);
        auto rt = ReturnInst::Create(ctx, ld, temp);
        // insert instruction metadata
        MDNode* md = MDNode::get(
            ctx, 
            ConstantAsMetadata::get(
                ConstantInt::get(
                    IntegerType::get(M.getContext(),32), PIMProfFakeBBID)
            )
        );
        rt->setMetadata(PIMProfBBIDMetadata, md);
    }
}


int main(int argc, char **argv) {
    // Module Construction
    Module *M = new Module("AnnotatorGeneration", ctx);

    CreateAnnotatorFunction(PIMProfAnnotatorHead, *M);
    CreateAnnotatorFunction(PIMProfAnnotatorTail, *M);

    std::error_code EC;
    raw_fd_ostream os(PIMProfAnnotatorFileName, EC,
        static_cast<sys::fs::OpenFlags>(0));
    WriteBitcodeToFile(*M, os);

  return 0;
}