#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/IRPrintingPasses.h"

#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"

#include "CFGDump.h"
#include "fstream"

using namespace llvm;

LLVMContext ctx;

int main(int argc, char **argv) {
    // Module Construction
    Module *M = new Module("AnnotatorGeneration", ctx);

  return 0;
}