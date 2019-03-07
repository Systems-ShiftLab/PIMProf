#include "llvm/Pass.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/LegacyPassManager.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

static const std::string annotatorName = "PIMProfAnnotator";

namespace {

    // Add the annotator declaration to each source file
    // so that the 
    // struct GenerateAnnotatorDecl : public ModulePass {
    //     static char ID;
    //     GenerateAnnotatorDecl() : ModulePass(ID) {}

    //     virtual bool runOnModule(Module &M) {
    //         std::vector<Type *> param(1, Type::getInt32Ty(context));
    //         FunctionType *FT = FunctionType::get(Type::getInt32Ty(context),
    //                                              param, false);
    //         // Function *F = Function::Create(FT, Function::ExternalLinkage,
    //         //                                annotatorName, &M);
    //         annotator = dyn_cast<Function>(
    //             M.getOrInsertFunction(
    //                 annotatorName, 
    //                 FunctionType::getVoidTy(context), 
    //                 Type::getInt32Ty(context)
    //             )
    //         );
    //     }

    // };

    struct InjectAnnotation : public BasicBlockPass {
        static char ID;
        InjectAnnotation() : BasicBlockPass(ID) {}

        virtual bool runOnBasicBlock(BasicBlock &BB) {
            Module *M = BB.getModule();
            LLVMContext &ctx = M->getContext();

            // get the handle of annotator function 
            Function *annotator = dyn_cast<Function>(
                M->getOrInsertFunction(
                    annotatorName, 
                    FunctionType::getVoidTy(ctx), 
                    Type::getInt32Ty(ctx)
                )
            );

            // // provide definition of function if it has not been defined 
            // if (annotator->empty()) {
            //     BasicBlock *temp = BasicBlock::Create(
            //         ctx, "", annotator, 0);
            //     ReturnInst::Create(ctx, temp);
            // }
         
            // assert(annotator);

            errs()  << "Before injection: " << BB.getParent()->getName()
                    << "::" << BB.getName() << "\n";
            for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
                (*i).print(errs());
                errs() << "\n";
            }
            errs() << "\n";

            Value *BBid = ConstantInt::get(
                IntegerType::get(M->getContext(),32), 1234);
            
            CallInst::Create(
                annotator, ArrayRef<Value *>(BBid), "",
                BB.getFirstNonPHIOrDbgOrLifetime());

            CallInst::Create(
                annotator, ArrayRef<Value *>(BBid), "",
                BB.getTerminator());


            errs() << "After injection: " << BB.getName() << "\n";
            for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
                (*i).print(errs());
                errs() << "\n";
            }
            errs() << "\n";


            return false;
        }
    };
}

// char GenerateAnnotatorDecl::ID = 0;
char InjectAnnotation::ID = 1;

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerLLVMInjection(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
    // PM.add(new GenerateAnnotatorDecl());
    PM.add(new InjectAnnotation());
}
static RegisterStandardPasses
  RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
                 registerLLVMInjection);
