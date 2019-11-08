//===- AnnotationInjection.cpp - Pass that injects BB annotator --*- C++ -*-===//
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

#include "MurmurHash3.h"
#include "Common.h"

using namespace llvm;

namespace {
    void InjectAnnotationCall(Module &M, BasicBlock &BB) {
        LLVMContext &ctx = M.getContext();

        // declare extern annotator function
        Function *annotator_head = dyn_cast<Function>(
            M.getOrInsertFunction(
                PIMProfAnnotationHead, 
                FunctionType::getInt64Ty(ctx), 
                Type::getInt64Ty(ctx),
                Type::getInt64Ty(ctx),
                Type::getInt64Ty(ctx)
            )
        );
        Function *annotator_tail = dyn_cast<Function>(
            M.getOrInsertFunction(
                PIMProfAnnotationTail, 
                FunctionType::getInt64Ty(ctx), 
                Type::getInt64Ty(ctx),
                Type::getInt64Ty(ctx),
                Type::getInt64Ty(ctx)
            )
        );

        // errs() << "Before injection: " << BB.getName() << "\n";
        // for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
        //     (*i).print(errs());
        //     errs() << "\n";
        // }
        // errs() << "\n";

        // use the content of BB itself as the hash key
        std::string BB_content;
        raw_string_ostream rso(BB_content);
        rso << BB;
        uint64_t bblhash[2];


        MurmurHash3_x64_128(BB_content.c_str(), BB_content.size(), 0, bblhash);
        // errs() << "Hash = " << bblhash[1] << " " << bblhash[0] << "\n";

        // divide all parameters into uint64_t, because this is what pin supports
        Value *hi = ConstantInt::get(
            IntegerType::get(M.getContext(), 64), bblhash[1]);
        Value *lo = ConstantInt::get(
            IntegerType::get(M.getContext(), 64), bblhash[0]);

        std::string funcname = BB.getParent()->getName();
        // errs() << funcname << "\n";
        // errs() << (funcname.find(OpenMPIdentifier) != std::string::npos) << "\n";

        Value *isomp = ConstantInt::get(
            IntegerType::get(M.getContext(), 64), 
            (funcname.find(OpenMPIdentifier) != std::string::npos));

        std::vector<Value *> arglist;
        arglist.push_back(hi);
        arglist.push_back(lo);
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
    }

    struct AnnotationInjection : public ModulePass {
        static char ID;
        AnnotationInjection() : ModulePass(ID) {}

        virtual bool runOnModule(Module &M) {
            // find all functions that are called by pthread
            // for (auto &func : M) {
            //     for (auto &bb : func) {
            //         for (auto &I : bb) {
            //             if(CallInst* call_inst = dyn_cast<CallInst>(&I)) {
            //                 Function *f = call_inst->getCalledFunction();
            //                 if (f->getName() == PThreadsIdentifier) {
            //                     Argument *arg = f->arg_begin();
            //                     arg += 2;
            //                     arg->print(errs());
            //                 }
            //             }
            //         }
            //     }
            // }

            // inject annotator function to each basic block
            // attach basic block id to terminator
            for (auto &func : M) {
                for (auto &bb: func) {
                    InjectAnnotationCall(M, bb);
                }
            }
            // M.print(errs(), nullptr);
            return true;
        }
    };
}

char AnnotationInjection::ID = 0;
static RegisterPass<AnnotationInjection> RegisterMyPass(
    "AnnotationInjection", "Inject annotators to uniquely identify each basic block.");

static void loadPass(const PassManagerBuilder &,
                           legacy::PassManagerBase &PM) {
    PM.add(new AnnotationInjection());
}
static RegisterStandardPasses clangtoolLoader_Ox(PassManagerBuilder::EP_OptimizerLast, loadPass);
static RegisterStandardPasses clangtoolLoader_O0(PassManagerBuilder::EP_EnabledOnOptLevel0, loadPass);
