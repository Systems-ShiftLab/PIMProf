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
#include <sstream>
#include <unordered_map>

#include "MurmurHash3.h"
#include "Common.h"

using namespace llvm;


namespace {
    enum CallSite {
        CPU, PIM, MAX_COST_SITE,
        INVALID = 0x3fffffff // a placeholder that does not count as a cost site
    };

    typedef std::pair<uint64_t, uint64_t> UUID;

    class HashFunc
    {
      public:
        // assuming UUID is already murmurhash-ed.
        std::size_t operator()(const UUID &key) const
        {
            size_t result = key.first ^ key.second;
            return result;
        }
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

    std::unordered_map<UUID, std::pair<CallSite, int>, HashFunc> decision_map;

    void InjectOffloaderCall(Module &M, BasicBlock &BB, int DECISION, int BBLID) {
        LLVMContext &ctx = M.getContext();

        // declare extern annotator function
        Function *offloader = dyn_cast<Function>(
            M.getOrInsertFunction(
                PIMProfOffloader, 
                FunctionType::getVoidTy(ctx), 
                Type::getInt64Ty(ctx)
            )
        );

        std::vector<Value *> args;
        args.push_back(ConstantInt::get(
            IntegerType::get(M.getContext(),64), DECISION));

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
                    IntegerType::get(M.getContext(),64), BBLID)
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
            if (StayOn == INVALID && InputFilename == "") {
                std::cerr << "Must specify either a place for the program to stay or give an input filename. Refer to -h.";
                assert(false);
            }
            else if (StayOn == INVALID && InputFilename != "") {
                // Read decisions from file
                std::ifstream ifs(InputFilename.c_str(), std::ifstream::in);
                std::string line;
                // skip the preceding lines
                for (int i = 0; i < 8; i++) {
                    std::getline(ifs, line);
                }
                while(std::getline(ifs, line)) {
                    std::stringstream ss(line);
                    std::string token;
                    CallSite decision;
                    int bblid;
                    uint64_t hi, lo;
                    for (int i = 0; i < 10; i++) {
                        if (i == 0) {
                            ss >> bblid;
                        }
                        else if (i == 1) {
                            ss >> token;
                            decision = (token == "P" ? PIM : CPU);
                        }
                        else if (i == 8) {
                            ss >> std::hex >> hi;
                        }
                        else if (i == 9) {
                            ss >> std::hex >> lo;
                        }
                        else {
                            // ignore it
                            ss >> token;
                        }
                    }
                    decision_map[UUID(hi, lo)] = std::make_pair(decision, bblid);
                }
                ifs.close();
                // inject offloader function to each basic block
                // according to their basic block ID and corresponding decision
                // simply assume that the input program is the same as the input in annotator injection pass
                for (auto &func : M) {
                    for (auto &bb: func) {
                        std::string BB_content;
                        raw_string_ostream rso(BB_content);
                        rso << bb;
                        uint64_t bblhash[2];
                        MurmurHash3_x64_128(BB_content.c_str(), BB_content.size(), 0, bblhash);
                        auto pair = decision_map[UUID(bblhash[1], bblhash[0])];
                        InjectOffloaderCall(M, bb, pair.first, pair.second);
                    }
                    errs() << "\n";
                }
            }
            else if (StayOn != INVALID && InputFilename == "") {
                for (auto &func : M) {
                    for (auto &bb: func) {
                        InjectOffloaderCall(M, bb, (int)StayOn, 0);
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
