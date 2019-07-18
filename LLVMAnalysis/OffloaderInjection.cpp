//===- OffloaderInjection.cpp - Pass that injects BB annotator --*- C++ -*-===//
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
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "Common.h"
#include "../PinInstrument/PinUtil.h"

using namespace llvm;

static cl::opt<std::string> InputFilename("decision", cl::desc("Specify filename of offloading decision for OffloaderInjection pass."), cl::value_desc("decision_file"));

namespace {
    void InjectOffloaderCall(Module &M, BasicBlock &BB, int BBLID) {
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
        Value *bblid = ConstantInt::get(
            IntegerType::get(M.getContext(),32), BBLID);

        // need to skip all PHIs and LandingPad instructions
        // check the declaration of getFirstInsertionPt()
        Instruction *beginning = &(*BB.getFirstInsertionPt());
        CallInst *head_instr = CallInst::Create(
            annotator_head, ArrayRef<Value *>(bblid), "",
            beginning);

        CallInst *tail_instr = CallInst::Create(
            annotator_tail, ArrayRef<Value *>(bblid), "",
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

    struct OffloaderInjection : public ModulePass {
        static char ID;
        OffloaderInjection() : ModulePass(ID) {}

        virtual bool runOnModule(Module &M) {
            std::vector<PIMProf::CostSite> decision;

            // inject annotator function to each basic block
            // attach basic block id to terminator
            for (auto &func : M) {
                for (auto &bb: func) {
                    InjectOffloaderCall(M, bb, bblid);
                    bblid++;
                }
            }
            // M.print(errs(), nullptr);
            return true;
        }
    };
}

char OffloaderInjection::ID = 0;
static RegisterPass<OffloaderInjection> RegisterMyPass(
    "OffloaderInjection", "Inject offloader when switching between CPU and PIM is required.");
