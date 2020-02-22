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
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/LLVMContext.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "MurmurHash3.h"
#include "Common.h"

// #include <iostream>

using namespace llvm;

namespace {
    void InjectAnnotationCall(Module &M, BasicBlock &BB) {
        LLVMContext &ctx = M.getContext();

        /***** create InlineAsm template ******/
        std::vector<Type *> argtype {
            Type::getInt64Ty(ctx), Type::getInt64Ty(ctx), Type::getInt64Ty(ctx)
        };
        FunctionType *asmty = FunctionType::get(
            Type::getVoidTy(ctx), argtype, false
        );
        InlineAsm *xchgIA = InlineAsm::get(
            asmty,
            "\txchg %rcx, %rcx\n"
            "\tmov $0, %rax \n"
            "\tmov $1, %rbx \n"
            "\tmov $2, %rcx \n",
            "imr,imr,imr,~{rax},~{rbx},~{rcx},~{dirflag},~{fpsr},~{flags}",
            true
        );

        /***** generate arguments for the InlineAsm ******/

        // use the content of BB itself as the hash key
        std::string BB_content;
        raw_string_ostream rso(BB_content);
        rso << BB;
        uint64_t bblhash[2];


        MurmurHash3_x64_128(BB_content.c_str(), BB_content.size(), 0, bblhash);

        // errs() << "Before injection: " << BB.getName() << "\n";
        // for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
        //     (*i).print(errs());
        //     errs() << "\n";
        // }
        // errs() << "\n";
        // errs() << "Hash = " << bblhash[1] << " " << bblhash[0] << "\n";

        // divide all parameters into uint64_t, because this is what pin supports
        Value *hi = ConstantInt::get(
            IntegerType::get(M.getContext(), 64), bblhash[1]);
        Value *lo = ConstantInt::get(
            IntegerType::get(M.getContext(), 64), bblhash[0]);


        std::string funcname = BB.getParent()->getName();
        uint64_t isomp = (funcname.find(OpenMPIdentifier) != std::string::npos);
        // std::cout << isomp << " " << ControlValue::GetControlValue(MAGIC_OP_ANNOTATIONHEAD, isomp) << std::endl;
        Value *control_head = ConstantInt::get(
            IntegerType::get(M.getContext(), 64), 
            ControlValue::GetControlValue(
                MAGIC_OP_ANNOTATIONHEAD, isomp)
        );
        Value *control_tail = ConstantInt::get(
            IntegerType::get(M.getContext(), 64), 
            ControlValue::GetControlValue(
                MAGIC_OP_ANNOTATIONTAIL, isomp)
        );

        std::vector<Value *> arglist_head {hi, lo, control_head};
        std::vector<Value *> arglist_tail {hi, lo, control_tail};

        // need to skip all PHIs and LandingPad instructions
        // check the declaration of getFirstInsertionPt()
        Instruction *beginning = &(*BB.getFirstInsertionPt());

        CallInst::Create(
            xchgIA, arglist_head, "", beginning);
        CallInst::Create(
            xchgIA, arglist_tail, "", BB.getTerminator());

        

        // errs() << "After injection: " << BB.getName() << "\n";
        // for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
        //     (*i).print(errs());
        //     errs() << "\n";
        // }
        // errs() << "\n";
        // errs() << "Hash = " << bblhash[1] << " " << bblhash[0] << "\n";
    }

    struct AnnotationInjection : public ModulePass {
        static char ID;
        AnnotationInjection() : ModulePass(ID) {}

        virtual bool runOnModule(Module &M) {
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
