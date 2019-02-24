#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
using namespace llvm;

namespace {
  struct BBLPass : public FunctionPass {
    static char ID;
    BBLPass() : FunctionPass(ID) {}

    virtual bool runOnFunction(Function &F) {
      errs() << "I saw a function called " << F.getName() << "!\n";
      return false;
    }
  };
}

char BBLPass::ID = 0;

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerBBLPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
  PM.add(new BBLPass());
}
static RegisterStandardPasses
  RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
                 registerBBLPass);
