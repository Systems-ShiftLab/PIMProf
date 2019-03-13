//===- AnnotatorInjection.cpp - Pass that injects BB annotator --*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

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
    void InjectAnnotatorCall(Module &M, BasicBlock &BB, int BBID) {
        LLVMContext &ctx = M.getContext();

        // declare extern annotator function
        Function *annotator_head = dyn_cast<Function>(
            M.getOrInsertFunction(
                PIMProfAnnotatorHead, 
                FunctionType::getInt32Ty(ctx), 
                Type::getInt32Ty(ctx)
            )
        );
        Function *annotator_tail = dyn_cast<Function>(
            M.getOrInsertFunction(
                PIMProfAnnotatorTail, 
                FunctionType::getInt32Ty(ctx), 
                Type::getInt32Ty(ctx)
            )
        );

        // errs() << "Before injection: " << BB.getName() << "\n";
        // for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
        //     (*i).print(errs());
        //     errs() << "\n";
        // }
        // errs() << "\n";

        // insert instruction
        Value *bbid = ConstantInt::get(
            IntegerType::get(M.getContext(),32), BBID);
        
        CallInst *head_instr = CallInst::Create(
            annotator_head, ArrayRef<Value *>(bbid), "",
            BB.getFirstNonPHIOrDbgOrLifetime());

        CallInst *tail_instr = CallInst::Create(
            annotator_tail, ArrayRef<Value *>(bbid), "",
            BB.getTerminator());
        
        // insert instruction metadata
        MDNode* md = MDNode::get(
            ctx, 
            ConstantAsMetadata::get(
                ConstantInt::get(
                    IntegerType::get(M.getContext(),32), BBID)
            )
        );
        head_instr->setMetadata("basicblock.id", md);
        tail_instr->setMetadata("basicblock.id", md);
            
        // errs() << "After injection: " << BB.getName() << "\n";
        // for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
        //     (*i).print(errs());
        //     errs() << "\n";
        // }
        // errs() << "\n";
    }

    struct AnnotatorInjection : public ModulePass {
        static char ID;
        AnnotatorInjection() : ModulePass(ID) {}

        virtual bool runOnModule(Module &M) {
            // assign unique id to each basic block
            int bbid = 0;

            for (auto &func : M) {
                for (auto &bb: func) {
                    // declare annotator function 
                    InjectAnnotatorCall(M, bb, bbid);
                    bbid++;
                }
            }
            // M.print(errs(), nullptr);
            return true;
        }
    };
}

char AnnotatorInjection::ID = 0;
static RegisterPass<AnnotatorInjection> RegisterMyPass(
    "AnnotatorInjection", "Inject annotators to uniquely identify each basic block.");

// // Automatically enable the pass.
// // http://adriansampson.net/blog/clangpass.html
// static void registerAnnotatorInjection(const PassManagerBuilder &,
//                          legacy::PassManagerBase &PM) {
//     PM.add(new AnnotatorInjection());
// }
// static RegisterStandardPasses
//   RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
//                  registerAnnotatorInjection);
