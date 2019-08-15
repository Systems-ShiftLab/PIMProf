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
    void InjectAnnotatorCall(Module &M, BasicBlock &BB, int BBLID) {
        LLVMContext &ctx = M.getContext();

        // declare extern annotator function
        Function *annotator_head = dyn_cast<Function>(
            M.getOrInsertFunction(
                PIMProfAnnotatorHead, 
                FunctionType::getInt32Ty(ctx), 
                Type::getInt32Ty(ctx),
                Type::getInt32Ty(ctx)
            )
        );
        Function *annotator_tail = dyn_cast<Function>(
            M.getOrInsertFunction(
                PIMProfAnnotatorTail, 
                FunctionType::getInt32Ty(ctx), 
                Type::getInt32Ty(ctx),
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
        Value *bblid = ConstantInt::get(
            IntegerType::get(M.getContext(),32), BBLID);

        std::string funcname = BB.getParent()->getName();
        // errs() << funcname << "\n";
        // errs() << (funcname.find(OpenMPIdentifier) != std::string::npos) << "\n";

        Value *isomp = ConstantInt::get(
            IntegerType::get(M.getContext(),32), 
            (funcname.find(OpenMPIdentifier) != std::string::npos));
        
        std::vector<Value *> arglist;
        arglist.push_back(bblid);
        arglist.push_back(isomp);

        // need to skip all PHIs and LandingPad instructions
        // check the declaration of getFirstInsertionPt()
        Instruction *beginning = &(*BB.getFirstInsertionPt());

        CallInst *head_instr = CallInst::Create(
            annotator_head, ArrayRef<Value *>(arglist), "",
            beginning);

        CallInst *tail_instr = CallInst::Create(
            annotator_tail, ArrayRef<Value *>(arglist), "",
            BB.getTerminator());
        // insert instruction metadata
        MDNode* md = MDNode::get(
            ctx, 
            ConstantAsMetadata::get(
                ConstantInt::get(
                    IntegerType::get(M.getContext(),32), BBLID)
            )
        );
        BB.getTerminator()->setMetadata(PIMProfBBLIDMetadata, md);
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
            int bblid = BBLStartingID;

            // inject annotator function to each basic block
            // attach basic block id to terminator
            for (auto &func : M) {
                for (auto &bb: func) {
                    InjectAnnotatorCall(M, bb, bblid);
                    bblid++;
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
