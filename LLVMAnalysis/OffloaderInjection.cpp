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

    struct Decision {
        CallSite decision;
        int bblid;
        double difference;
    };

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

    std::unordered_map<UUID, Decision, HashFunc> decision_map;

    void InjectOffloaderCall(Module &M, BasicBlock &BB) {
        Decision decision;
        std::string BB_content;
        raw_string_ostream rso(BB_content);
        rso << BB;
        uint64_t bblhash[2];
        MurmurHash3_x64_128(BB_content.c_str(), BB_content.size(), 0, bblhash);

        errs() << "Before offloader injection: " << BB.getName() << "\n";
        for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
            (*i).print(errs());
            errs() << "\n";
        }
        errs() << "\n";
        errs() << "Hash = " << bblhash[1] << " " << bblhash[0] << "\n";

        UUID uuid(bblhash[1], bblhash[0]);
        if (decision_map.find(uuid) == decision_map.end()) {
            std::cerr << "cannot find same function." << std::endl;
            // assert(false);
            decision.decision = CPU;
        }
        else {
            std::cerr << "found same function." << std::endl;
            decision = decision_map[uuid];
        }

        LLVMContext &ctx = M.getContext();

        // declare extern annotator function
        Function *offloader = dyn_cast<Function>(
            M.getOrInsertFunction(
                PIMProfOffloaderName, 
                FunctionType::getInt32Ty(ctx),
                Type::getInt32Ty(ctx),
                Type::getInt32Ty(ctx)
            )
        );

        // divide all parameters into uint64_t, because this is what pin supports
        Value *dec = ConstantInt::get(
            IntegerType::get(M.getContext(), 32), decision.decision);
        Value *mode0 = ConstantInt::get(
            IntegerType::get(M.getContext(), 32), 0);
        Value *mode1 = ConstantInt::get(
            IntegerType::get(M.getContext(), 32), 1);

        std::vector<Value *> arglist;
        arglist.push_back(dec);
        arglist.push_back(mode0);
        // need to skip all PHIs and LandingPad instructions
        // check the declaration of getFirstInsertionPt()
        Instruction *beginning = &(*BB.getFirstInsertionPt());

        CallInst *head_instr = CallInst::Create(
            offloader, ArrayRef<Value *>(arglist), "",
            beginning);

        arglist.clear();
        arglist.push_back(dec);
        arglist.push_back(mode1);

        CallInst *tail_instr = CallInst::Create(
            offloader, ArrayRef<Value *>(arglist), "",
            BB.getTerminator());
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
            char *InputFilename = std::getenv(PIMProfEnvName.c_str());

            std::cout << "filename: " << InputFilename << std::endl;
            std::ifstream ifs(InputFilename, std::ifstream::in);
            std::string line;
            // skip the preceding lines
            for (int i = 0; i < 8; i++) {
                std::getline(ifs, line);
            }
            while(std::getline(ifs, line)) {
                std::stringstream ss(line);
                std::string token;
                Decision decision;
                uint64_t hi, lo;
                for (int i = 0; i < 10; i++) {
                    if (i == 0) {
                        ss >> decision.bblid;
                    }
                    else if (i == 1) {
                        ss >> token;
                        decision.decision = (token == "P" ? PIM : CPU);
                    }
                    else if (i == 7) {
                        ss >> decision.difference;
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
                decision_map[UUID(hi, lo)] = decision;
            }
            ifs.close();
            // inject offloader function to each basic block
            // according to their basic block ID and corresponding decision
            // simply assume that the input program is the same as the input in annotator injection pass
            for (auto &func : M) {
                for (auto &bb: func) {
                    // errs() << "Before offloading: " << bb.getName() << "\n";
                    // for (auto i = bb.begin(), ie = bb.end(); i != ie; i++) {
                    //     (*i).print(errs());
                    //     errs() << "\n";
                    // }
                    // errs() << "\n";
                    InjectOffloaderCall(M, bb);
                }
                errs() << "\n";
            }

            // M.print(errs(), nullptr);
            return true;
        }
    };
}

char OffloaderInjection::ID = 0;
static RegisterPass<OffloaderInjection> RegisterMyPass(
    "OffloaderInjection", "Inject offloader when switching between CPU and PIM is required.");

static void loadPass(const PassManagerBuilder &,
                           legacy::PassManagerBase &PM) {
    PM.add(new OffloaderInjection());
}
static RegisterStandardPasses clangtoolLoader_Ox(PassManagerBuilder::EP_OptimizerLast, loadPass);
static RegisterStandardPasses clangtoolLoader_O0(PassManagerBuilder::EP_EnabledOnOptLevel0, loadPass);
