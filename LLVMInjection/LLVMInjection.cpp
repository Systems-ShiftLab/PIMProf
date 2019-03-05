#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
using namespace llvm;

namespace {
    // Add the annotator declaration to each source file
    // so th
    struct GenerateAnnotatorDecl : public ModulePass {
        static char ID;
        GenerateAnnotatorDecl() : ModulePass(ID) {}

        virtual bool runOnModule(Module &M) {
            FunctionType *FT = FunctionType::get(Type::getVoidTy(getGlobalContext()), Doubles, false);
            Function *F = Function::Create(
                FT,
                Function::ExternalLinkage,
                "PIMProfAnnotator",
                &M);
        }

        virtual bool runOnBasicBlock(BasicBlock &BB) {
            Instruction *instr_start;
            Instruction *instr_end;
            BB.getInstList().insert(BB.getFirstInsertionPt(), instr_start);
            BB.getInstList().insert(BB.end(), instr_end);
            errs() << "Insert to BBL " << BB.getName() << "!\n";
            return false;
        }
    };

    struct InjectAnnotation : public BasicBlockPass {
        static char ID;
        InjectAnnotation() : BasicBlockPass(ID) {}

        virtual bool runOnBasicBlock(BasicBlock &BB) {
            Instruction *instr_start;
            Instruction *instr_end;
            BB.getInstList().insert(BB.getFirstInsertionPt(), instr_start);
            BB.getInstList().insert(BB.end(), instr_end);
            errs() << "Insert to BBL " << BB.getName() << "!\n";
            return false;
        }
    };
}

char GenerateAnnotatorDecl::ID = 0;
char InjectAnnotation::ID = 1;

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerLLVMInjection(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
    PM.add(new GenerateAnnotatorDecl());
    PM.add(new InjectAnnotation());
}
static RegisterStandardPasses
  RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
                 registerLLVMInjection);
