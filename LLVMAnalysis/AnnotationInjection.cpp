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

namespace { // Anonymous namespace
/***
 * Format of magical instructions:
 * 
 * xchg %rcx, %rcx
 * mov <higher bits of the UUID>, %rax
 * mov <lower bits of the UUID>, %rbx
 * mov <the control bits>, %rcx
 * 
 * The magical instructions should all be skipped when analyzing performance
***/
void InsertPIMProfMagic(Module &M, uint64_t arg0, uint64_t arg1, uint64_t arg2, Instruction *insertPt)
{
    LLVMContext &ctx = M.getContext();
    std::vector<Type *> argtype {
        Type::getInt64Ty(ctx), Type::getInt64Ty(ctx), Type::getInt64Ty(ctx)
    };
    FunctionType *ty = FunctionType::get(
        Type::getVoidTy(ctx), argtype, false
    );
    // template of Sniper's SimMagic0
    InlineAsm *ia = InlineAsm::get(
        ty,
        "\txchg %rcx, %rcx\n"
        "\tmov $0, %rax \n"
        "\tmov $1, %rbx \n"
        "\tmov $2, %rcx \n",
        "imr,imr,imr,~{rax},~{rbx},~{rcx},~{dirflag},~{fpsr},~{flags}",
        true
    );
    Value *val0 = ConstantInt::get(IntegerType::get(ctx, 64), arg0);
    Value *val1 = ConstantInt::get(IntegerType::get(ctx, 64), arg1);
    Value *val2 = ConstantInt::get(IntegerType::get(ctx, 64), arg2);
    std::vector<Value *> arglist {val0, val1, val2};
    CallInst::Create(
            ia, arglist, "", insertPt);
}

void InjectAnnotationCall(Module &M, BasicBlock &BB) {

    /***** generate arguments for the InlineAsm ******/

    // use the content of BB itself as the hash key
    std::string BB_content;
    raw_string_ostream rso(BB_content);
    rso << BB;
    uint64_t bblhash[2];


    MurmurHash3_x64_128(BB_content.c_str(), BB_content.size(), 0, bblhash);

    // errs() << "Before annotator injection: " << BB.getName() << "\n";
    // for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
    //     (*i).print(errs());
    //     errs() << "\n";
    // }
    // errs() << "\n";
    // errs() << "Hash = " << bblhash[1] << " " << bblhash[0] << "\n";

    std::string funcname = BB.getParent()->getName();
    uint64_t isomp = (funcname.find(OpenMPIdentifier) != std::string::npos);

    uint64_t control_head = ControlValue::GetControlValue(MAGIC_OP_ANNOTATIONHEAD, isomp);
    uint64_t control_tail = ControlValue::GetControlValue(MAGIC_OP_ANNOTATIONTAIL, isomp);

    // need to skip all PHIs and LandingPad instructions
    // check the declaration of getFirstInsertionPt()
    Instruction *beginning = &(*BB.getFirstInsertionPt());

    InsertPIMProfMagic(M, bblhash[1], bblhash[0], control_head, beginning);
    InsertPIMProfMagic(M, bblhash[1], bblhash[0], control_tail, BB.getTerminator());

    // errs() << "After annotator injection: " << BB.getName() << "\n";
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

} // Anonymous namespace

char AnnotationInjection::ID = 0;
static RegisterPass<AnnotationInjection> RegisterMyPass(
    "AnnotationInjection", "Inject annotators to uniquely identify each basic block.");

static void loadPass(const PassManagerBuilder &,
                           legacy::PassManagerBase &PM) {
    PM.add(new AnnotationInjection());
}
static RegisterStandardPasses clangtoolLoader_Ox(PassManagerBuilder::EP_OptimizerLast, loadPass);
static RegisterStandardPasses clangtoolLoader_O0(PassManagerBuilder::EP_EnabledOnOptLevel0, loadPass);
