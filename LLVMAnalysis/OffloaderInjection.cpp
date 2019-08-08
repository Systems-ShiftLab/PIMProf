//===- OffloaderInjection.cpp - Pass that injects BB annotator --*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"

#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/LLVMContext.h"

#include "llvm/IR/AssemblyAnnotationWriter.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"


#include <iostream>
#include <assert.h>
#include <fstream>

#include "Common.h"

using namespace llvm;


namespace {
    enum CallSite {
        CPU = 0,
        PIM = 1,
        INVALID = 0x3fffffff // a placeholder that does not count as a cost site
    };

    static cl::opt<CallSite> StayOn(
        cl::desc("Instead of specifying a filename, specify that the entire program will stay in the same place"),
        cl::values(
            clEnumVal(CPU, "Entire program will stay on CPU"),
            clEnumVal(PIM, "Entire program will stay on PIM")
        ),
        cl::init(INVALID)
    );

    static cl::opt<std::string> InputFilename(
        "decision",
        cl::desc("Specify filename of offloading decision for OffloaderInjection pass."),
        cl::value_desc("decision file"),
        cl::init("")
    );

    void InjectOffloaderCall(Module &M, BasicBlock &BB, int DECISION, int BBLID) {
        LLVMContext &ctx = M.getContext();

        // declare extern annotator function
        Function *offloader = dyn_cast<Function>(
            M.getOrInsertFunction(
                PIMProfOffloader, 
                FunctionType::getVoidTy(ctx), 
                Type::getInt32Ty(ctx),
                Type::getInt32Ty(ctx)
            )
        );

        std::vector<Value *> args;
        args.push_back(ConstantInt::get(
            IntegerType::get(M.getContext(),32), DECISION));
        args.push_back(ConstantInt::get(
            IntegerType::get(M.getContext(),32), BBLID));

        // need to skip all PHIs and LandingPad instructions
        // check the declaration of getFirstInsertionPt()
        Instruction *beginning = &(*BB.getFirstInsertionPt());
        CallInst *head_instr = CallInst::Create(
            offloader, ArrayRef<Value *>(args), "",
            beginning);

        // insert instruction metadata
        MDNode* md = MDNode::get(
            ctx, 
            ConstantAsMetadata::get(
                ConstantInt::get(
                    IntegerType::get(M.getContext(),32), BBLID)
            )
        );
        head_instr->setMetadata(PIMProfBBLIDMetadata, md);

    }

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

    struct OffloaderInjection : public ModulePass {
        static char ID;
        OffloaderInjection() : ModulePass(ID) {}

        virtual bool runOnModule(Module &M) {
            // assign unique id to each basic block
            int bblid = BBLStartingID;
            std::vector<int> decisions;
            for (int i = 0; i < bblid; i++) {
                decisions.push_back(-1);
            }
            
            if (StayOn == INVALID && InputFilename == "") {
                std::cerr << "Must specify either a place for the program to stay or give an input filename. Refer to -h.";
                assert(false);
            }
            else if (StayOn == INVALID && InputFilename != "") {
                // Read decisions from file
                std::ifstream ifs(InputFilename.c_str(), std::ifstream::in);
                int temp;
                std::string tempdecision;
                while(ifs >> temp >> tempdecision) {
                    if (tempdecision == "C") {
                        decisions.push_back(0);
                    }
                    else if (tempdecision == "P") {
                        decisions.push_back(1);
                    }
                    else {
                        assert(false);
                    }
                }
                ifs.close();
                // inject offloader function to each basic block
                // according to their basic block ID and corresponding decision
                // simply assume that the input program is the same as the input in annotator injection pass
                for (auto &func : M) {
                    errs() << func.getName() << "\n";
                    for (auto &bb: func) {
                        errs() << bblid << "(" << decisions[bblid] << ") ";
                        InjectOffloaderCall(M, bb, decisions[bblid], bblid);
                        bblid++;
                    }
                    errs() << "\n";
                }
            }
            else if (StayOn != INVALID && InputFilename == "") {
                for (auto &func : M) {
                    for (auto &bb: func) {
                        InjectOffloaderCall(M, bb, (int)StayOn, bblid);
                        bblid++;
                    }
                }
            }
            else {
                std::cerr << "Can only specify either the place for the program to stay or the input filename.";
                assert(false);
            }
            
            // dump entire program
            PIMProfAAW aaw = PIMProfAAW();
            for (auto &func: M) {
                func.print(outs(), &aaw);
            }

            // M.print(errs(), nullptr);
            return true;
        }
    };
}

char OffloaderInjection::ID = 0;
static RegisterPass<OffloaderInjection> RegisterMyPass(
    "OffloaderInjection", "Inject offloader when switching between CPU and PIM is required.");
