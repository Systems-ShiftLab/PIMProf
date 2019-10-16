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

#include "MurmurHash3.h"
#include "Common.h"
#include <iostream>

using namespace llvm;

namespace {
    void InjectAnnotatorCall(Module &M, BasicBlock &BB) {
        LLVMContext &ctx = M.getContext();

        // declare extern annotator function
        Function *annotator_head = dyn_cast<Function>(
            M.getOrInsertFunction(
                PIMProfAnnotatorHead, 
                FunctionType::getInt64Ty(ctx), 
                Type::getInt64Ty(ctx),
                Type::getInt64Ty(ctx),
                Type::getInt64Ty(ctx)
            )
        );
        Function *annotator_tail = dyn_cast<Function>(
            M.getOrInsertFunction(
                PIMProfAnnotatorTail, 
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
        std::cout << std::hex << bblhash[1] << " " << bblhash[0] << std::endl;

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
        // insert instruction metadata
        // MDNode* md = MDNode::get(
        //     ctx, 
        //     ConstantAsMetadata::get(
        //         ConstantInt::get(
        //             IntegerType::get(M.getContext(), 64), BBLHash)
        //     )
        // );
        // BB.getTerminator()->setMetadata(PIMProfBBLIDMetadata, md);

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

            // inject annotator function to each basic block
            // attach basic block id to terminator
            for (auto &func : M) {
                for (auto &bb: func) {
                    InjectAnnotatorCall(M, bb);
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
