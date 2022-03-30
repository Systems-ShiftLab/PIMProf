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

void InjectPIMProfMagic(Module &M, Instruction *insertPt, uint64_t arg0, uint64_t arg1, uint64_t arg2)
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
void InjectSimMagic0(Module &M, Instruction *insertPt, uint64_t arg0)
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

void InjectSimMagic2(Module &M, Instruction *insertPt, uint64_t arg0, uint64_t arg1, uint64_t arg2)
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

void InjectFunctionCall2(Module &M, Instruction *insertPt, const std::string &funcName, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    LLVMContext &ctx = M.getContext();

    Function *func = dyn_cast<Function>(
        M.getOrInsertFunction(
            funcName,
            FunctionType::getInt64Ty(ctx),
            Type::getInt64Ty(ctx),
            Type::getInt64Ty(ctx),
            Type::getInt64Ty(ctx)
        ).getCallee()
    );

    Value *val0 = ConstantInt::get(
        IntegerType::get(M.getContext(), 64), arg0);
    Value *val1 = ConstantInt::get(
        IntegerType::get(M.getContext(), 64), arg1);
    Value *val2 = ConstantInt::get(
        IntegerType::get(M.getContext(), 64), arg2);

    std::vector<Value *> arglist{val0, val1, val2};

    CallInst *call = CallInst::Create(
        func, ArrayRef<Value *>(arglist), "", insertPt);
}


/*
void InjectVTuneITT(Module &M, VTUNE_MODE mode, Instruction *insertPt) {
    LLVMContext &ctx = M.getContext();

    // create ___itt_domain (typedef'ed as __itt_domain)
    // %struct.___itt_domain = type { i32, i8*, i8*, i32, i8*, %struct.___itt_domain* }
    StructType *ittDomainType = StructType::create(ctx, "___itt_domain");
    ittDomainType->setBody(
        {Type::getInt32Ty(ctx), Type::getInt8PtrTy(ctx), Type::getInt8PtrTy(ctx),
        Type::getInt32Ty(ctx), Type::getInt8PtrTy(ctx), PointerType::get(ittDomainType, 0)}
    );

    // create a new domain and set flags to 1
    // __itt_domain* pD = __itt_domain_create("PIMProf_Domain");
    // pD->flags = 1;
    if (mode == VTUNE_MODE_CREATE_DOMAIN) {
        GlobalVariable *gvar = dyn_cast<GlobalVariable>(
            M.getOrInsertGlobal("PIMProf_Domain", ittDomainType));
        FunctionCallee func = M.getOrInsertFunction(
                "__itt_domain_create",
                FunctionType::getInt64PtrTy(ctx),
                Type::getInt8PtrTy(ctx));
        std::vector<Value *> arglist{};

        // insert the following instructions in a reversed order
        // 
        CallInst *call = CallInst::Create(func, arglist, "", insertPt); // 
        LoadInst *load = new LoadInst(ittDomainType, dyn_cast<Value>(func.getCallee()), "", insertPt);
    }

    if (mode == VTUNE_MODE_RESUME) {
        FunctionCallee *func = dyn_cast<FunctionCallee>(
            M.getOrInsertFunction(
                "__itt_resume",
                FunctionType::getVoidTy(ctx),
                Type::getVoidTy(ctx)));
        std::vector<Value *> arglist{};
        CallInst::Create(func, arglist, "", insertPt);
    }
    if (mode == VTUNE_MODE_PAUSE) {
        FunctionCallee *func = dyn_cast<FunctionCallee>(
            M.getOrInsertFunction(
                "__itt_pause",
                FunctionType::getVoidTy(ctx),
                Type::getVoidTy(ctx)));
        std::vector<Value *> arglist{};
        CallInst::Create(func, arglist, "", insertPt);
    }
    if (mode == VTUNE_MODE_DETACH) {
        FunctionCallee *func = dyn_cast<FunctionCallee>(
            M.getOrInsertFunction(
                "__itt_detach",
                FunctionType::getVoidTy(ctx),
                Type::getVoidTy(ctx)));
        std::vector<Value *> arglist{};
        CallInst::Create(func, arglist, "", insertPt);
    }

    if (mode == VTUNE_MODE_FRAME_BEGIN) {
        FunctionCallee *func = dyn_cast<FunctionCallee>(
            M.getOrInsertFunction(
                "__itt_frame_begin_v3",
                FunctionType::getVoidTy(ctx),
                Type::getVoidTy(ctx)));
    }
    if (mode == VTUNE_MODE_FRAME_END) {
         Function *func = dyn_cast<Function>(
            M.getOrInsertFunction(
                "__itt_frame_end_v3",
                FunctionType::getVoidTy(ctx),
                Type::getVoidTy(ctx)));
    }
}
*/

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


void LLVMPrint(const BasicBlock &BB, raw_ostream &out) {
    out << "PrintBB: " << BB.getName() << "\n";
    for (const auto &I : BB) {
        I.print(out);
        out << "\n";
    }
    out << "\n";
}

void LLVMPrint(const Function &F, raw_ostream &out) {
    out << "PrintF: " << F.getName() << "\n";
    for (const auto &I : F) {
        I.print(out);
        out << "\n";
    }
    out << "\n";
}

} // namespace PIMProf