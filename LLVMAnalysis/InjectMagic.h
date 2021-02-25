//===- InjectMagic.h - Functions for inserting magic to module  --*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/LLVMContext.h"

#include "llvm/IR/AssemblyAnnotationWriter.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/FormattedStream.h"

#include "Common.h"

using namespace llvm;

namespace PIMProf {
/********************************************************
* PIMProf
*
* Format of magical instructions:
* 
* xchg %rcx, %rcx
* mov <higher bits of the UUID>, %rax
* mov <lower bits of the UUID>, %rbx
* mov <the control bits>, %rcx
* 
* The magical instructions should all be skipped when analyzing performance
* So we put xchg at the beginning to skip all following instructions
********************************************************/

void InjectPIMProfMagic(Module &M, uint64_t arg0, uint64_t arg1, uint64_t arg2, Instruction *insertPt)
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

/********************************************************
* Sniper
********************************************************/

// Inject Sniper's SimMagic0 before the LLVM::Instruction pointed by injectPt
void InjectSimMagic0(Module &M, uint64_t arg0, Instruction *insertPt)
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
        "\tmov $0, %rax \n"
        "\txchg %bx, %bx\n",
        "imr,~{rax},~{dirflag},~{fpsr},~{flags}",
        true
    );
    Value *val0 = ConstantInt::get(IntegerType::get(M.getContext(), 64), arg0);
    std::vector<Value *> arglist {val0};
    CallInst::Create(
            ia, arglist, "", insertPt);
}

void InjectSimMagic2(Module &M, uint64_t arg0, uint64_t arg1, uint64_t arg2, Instruction *insertPt)
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
        "\tmov $0, %rax \n"
        "\tmov $1, %rbx \n"
        "\tmov $2, %rcx \n"
        "\txchg %bx, %bx\n",
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

/********************************************************
* VTune
********************************************************/

void InjectVTuneITT(Module &M, VTUNE_MODE mode, Instruction *insertPt) {
    LLVMContext &ctx = M.getContext();
    Function *offloader = dyn_cast<Function>(
        M.getOrInsertFunction(
            VTuneOffloaderName, 
            FunctionType::getInt32Ty(ctx),
            Type::getInt32Ty(ctx)
        )
    );
    Value *val0 = ConstantInt::get(IntegerType::get(M.getContext(), 32), mode);
    std::vector<Value *> arglist {val0};
    CallInst::Create(
        offloader, arglist, "", insertPt);
}

/********************************************************
* Assembly annotation writer
********************************************************/

class PIMProfAAW : public AssemblyAnnotationWriter {
public:
    void emitInstructionAnnot(const Instruction *I, formatted_raw_ostream &ofs) {
        ofs << "######## At ";
        DILocation *deb = I->getDebugLoc();
        if (deb != NULL) {
            ofs << deb->getFilename();
            ofs << " line: " << deb->getLine();
            ofs << " col: " << deb->getColumn();
        }
        ofs << "\n";
    }
};

} // namespace PIMProf