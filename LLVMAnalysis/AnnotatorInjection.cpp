#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/LLVMContext.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "Common.h"

using namespace llvm;

namespace {

    struct InjectAnnotation : public BasicBlockPass {
        static char ID;
        InjectAnnotation() : BasicBlockPass(ID) {}

        virtual bool runOnBasicBlock(BasicBlock &BB) {
            Module *M = BB.getModule();
            LLVMContext &ctx = M->getContext();

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

            Value *BBid = ConstantInt::get(
                IntegerType::get(M->getContext(),32), 1234);
            
            CallInst *head_instr = CallInst::Create(
                annotator_head, ArrayRef<Value *>(BBid), "",
                BB.getFirstNonPHIOrDbgOrLifetime());

            CallInst *tail_instr = CallInst::Create(
                annotator_tail, ArrayRef<Value *>(BBid), "",
                BB.getTerminator());


            // errs() << "After injection: " << BB.getName() << "\n";
            // for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
            //     (*i).print(errs());
            //     errs() << "\n";
            // }
            // errs() << "\n";

            return true;
        }
    };
}

char InjectAnnotation::ID = 0;

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerAnnotatorInjection(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
    PM.add(new InjectAnnotation());
}
static RegisterStandardPasses
  RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
                 registerAnnotatorInjection);
