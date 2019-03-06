#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
using namespace llvm;



static const std::string annotatorName = "PIMProfAnnotator";

namespace {
    // Add the annotator declaration to each source file
    // so that the 
    struct GenerateAnnotatorDecl : public ModulePass {
        static char ID;
        GenerateAnnotatorDecl() : ModulePass(ID) {}

        virtual bool runOnModule(Module &M) {
            std::vector<Type *> param(1, Type::getInt32Ty(M.getContext()));
            FunctionType *FT = FunctionType::get(Type::getInt32Ty(M.getContext()),
                                                 param, false);
            // Function *F = Function::Create(FT, Function::ExternalLinkage,
            //                                annotatorName, &M);
            // Constant *c = M.getOrInsertFunction(
            //                 annotatorName, 
            //                 FunctionType::getVoidTy(M.getContext()), 
            //                 Type::getInt32Ty(M.getContext()), 
            //                 NULL);
        }

    };

    struct InjectAnnotation : public BasicBlockPass {
        static char ID;
        InjectAnnotation() : BasicBlockPass(ID) {}

        virtual bool runOnBasicBlock(BasicBlock &BB) {
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
