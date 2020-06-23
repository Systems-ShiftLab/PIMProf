//===- AnnotationInjection.cpp - Pass that injects BB annotator --*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"

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
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"


#include <iostream>
#include <assert.h>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "MurmurHash3.h"
#include "Common.h"

// #include <iostream>

#define SIM_CMD_ROI_START       1
#define SIM_CMD_ROI_END         2

#define SIM_PIM_OFFLOAD_START  15
#define SIM_PIM_OFFLOAD_END    16

using namespace llvm;

namespace {
    enum CallSite {
        CPU, PIM, MAX_COST_SITE,
        DEFAULT = 0x0fffffff,
        INVALID = 0x3fffffff // a placeholder that does not count as a cost site
    };

    typedef std::pair<uint64_t, uint64_t> UUID;

    struct Decision {
        CallSite decision;
        int bblid;
        double difference;
        int parallel;
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
    int found_cnt = 0, not_found_cnt = 0;
    int insert_cnt = 0;

    CallSite ROI = DEFAULT;

    void InjectOffloaderCall(Module &M, BasicBlock &BB) {
        Decision decision;

        LLVMContext &ctx = M.getContext();

        /***** create InlineAsm template ******/
        std::vector<Type *> argtype {
            Type::getInt64Ty(ctx), Type::getInt64Ty(ctx), Type::getInt64Ty(ctx)
        };
        FunctionType *asmty = FunctionType::get(
            Type::getVoidTy(ctx), argtype, false
        );
        // InlineAsm *xchgIA = InlineAsm::get(
        //     asmty,
        //     "\txchg %rcx, %rcx\n"
        //     "\tmov $0, %rax \n"
        //     "\tmov $1, %rbx \n"
        //     "\tmov $2, %rcx \n",
        //     "imr,imr,imr,~{rax},~{rbx},~{rcx},~{dirflag},~{fpsr},~{flags}",
        //     true
        // );

        // template of Sniper's SimMagic0
        InlineAsm *xchgIA = InlineAsm::get(
            asmty,
            "\tmov $0, %rax \n"
            "\txchg %bx, %bx\n",
            "imr,~{rax},~{dirflag},~{fpsr},~{flags}",
            true
        );

        /***** generate arguments for the InlineAsm ******/

        // use the content of BB itself as the hash key
        std::string BB_content;
        raw_string_ostream rso(BB_content);
        rso << BB;
        uint64_t bblhash[2];


        MurmurHash3_x64_128(BB_content.c_str(), BB_content.size(), 0, bblhash);

        // errs() << "Before offloader injection: " << BB.getName() << "\n";
        // for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
        //     (*i).print(errs());
        //     errs() << "\n";
        // }
        // errs() << "\n";
        // errs() << "Hash = " << bblhash[1] << " " << bblhash[0] << "\n";

        UUID uuid(bblhash[1], bblhash[0]);
        if (decision_map.find(uuid) == decision_map.end()) {
            not_found_cnt++;
            return;
        }
        else {
            found_cnt++;
            // std::cerr << "found same function." << std::endl;
            decision = decision_map[uuid];
            // only PIM regions need to be annotated
            if (decision.decision != PIM) return;
        }

        // divide all parameters into uint64_t, because this is what pin supports
        Value *hi = ConstantInt::get(
            IntegerType::get(M.getContext(), 64), bblhash[1]);
        Value *lo = ConstantInt::get(
            IntegerType::get(M.getContext(), 64), bblhash[0]);


        std::string funcname = BB.getParent()->getName();
        uint64_t isomp = (funcname.find(OpenMPIdentifier) != std::string::npos);
        // std::cout << isomp << " " << ControlValue::GetControlValue(MAGIC_OP_ANNOTATIONHEAD, isomp) << std::endl;
        Value *offload_head = ConstantInt::get(
            IntegerType::get(M.getContext(), 64), 
            SIM_PIM_OFFLOAD_START
        );
        Value *offload_tail = ConstantInt::get(
            IntegerType::get(M.getContext(), 64), 
            SIM_PIM_OFFLOAD_END
        );

        std::vector<Value *> arglist_offload_head {offload_head};
        std::vector<Value *> arglist_offload_tail {offload_tail};

        // need to skip all PHIs and LandingPad instructions
        // check the declaration of getFirstInsertionPt()
        Instruction *beginning = &(*BB.getFirstInsertionPt());
        
        if (ROI == CPU) {
            CallInst::Create(
                xchgIA, arglist_offload_tail, "", beginning);
            CallInst::Create(
                xchgIA, arglist_offload_head, "", BB.getTerminator());
        }
        else {
            CallInst::Create(
                xchgIA, arglist_offload_head, "", beginning);
            CallInst::Create(
                xchgIA, arglist_offload_tail, "", BB.getTerminator());
        }
        insert_cnt++;

        

        // errs() << "After injection: " << BB.getName() << "\n";
        // for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
        //     (*i).print(errs());
        //     errs() << "\n";
        // }
        // errs() << "\n";
        // errs() << "Hash = " << bblhash[1] << " " << bblhash[0] << "\n";
    }

    void InjectOffloaderCall(Module &M, Function &F) {
        LLVMContext &ctx = M.getContext();

        /***** create InlineAsm template ******/
        std::vector<Type *> argtype {
            Type::getInt64Ty(ctx), Type::getInt64Ty(ctx), Type::getInt64Ty(ctx)
        };
        FunctionType *asmty = FunctionType::get(
            Type::getVoidTy(ctx), argtype, false
        );
        // InlineAsm *xchgIA = InlineAsm::get(
        //     asmty,
        //     "\txchg %rcx, %rcx\n"
        //     "\tmov $0, %rax \n"
        //     "\tmov $1, %rbx \n"
        //     "\tmov $2, %rcx \n",
        //     "imr,imr,imr,~{rax},~{rbx},~{rcx},~{dirflag},~{fpsr},~{flags}",
        //     true
        // );

        InlineAsm *xchgIA = InlineAsm::get(
            asmty,
            "\tmov $0, %rax \n"
            "\txchg %bx, %bx\n",
            "imr,~{rax},~{dirflag},~{fpsr},~{flags}",
            true
        );

        Value *roi_head = ConstantInt::get(
            IntegerType::get(M.getContext(), 64), 
            SIM_CMD_ROI_START
        );
        Value *roi_tail = ConstantInt::get(
            IntegerType::get(M.getContext(), 64), 
            SIM_CMD_ROI_END
        );

        std::vector<Value *> arglist_roi_head {roi_head};
        std::vector<Value *> arglist_roi_tail {roi_tail};

        Value *offload_head = ConstantInt::get(
            IntegerType::get(M.getContext(), 64), 
            SIM_PIM_OFFLOAD_START
        );
        Value *offload_tail = ConstantInt::get(
            IntegerType::get(M.getContext(), 64), 
            SIM_PIM_OFFLOAD_END
        );

        std::vector<Value *> arglist_offload_head {offload_head};
        std::vector<Value *> arglist_offload_tail {offload_tail};

        // need to skip all PHIs and LandingPad instructions
        // check the declaration of getFirstInsertionPt()
        Instruction *beginning = &(*F.getEntryBlock().getFirstInsertionPt());

        CallInst::Create(
            xchgIA, arglist_roi_head, "",
            beginning);

        if (ROI == CPU) {
            CallInst::Create(
                xchgIA, arglist_offload_head, "",
                beginning);
        }

        // insert an end call before every return instruction
        for (auto &BB : F) {
            for (auto &I : BB) {
                if (isa<ReturnInst>(I)) {
                    if (ROI == CPU) {
                        CallInst::Create(
                            xchgIA, arglist_offload_tail, "",
                            &I);
                    }
                    CallInst::Create(
                        xchgIA, arglist_roi_tail, "",
                        &I);
                }
            }
        }

        
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
            char *decisionfileenv = NULL;
            char *roienv = NULL;
            decisionfileenv = std::getenv(PIMProfDecisionEnv.c_str());
            roienv = std::getenv(PIMProfROIEnv.c_str());
            assert(decisionfileenv != NULL);
            std::cout << "filename: " << decisionfileenv << std::endl;
            std::cout << "roi: " << roienv << std::endl;
            std::ifstream ifs(decisionfileenv, std::ifstream::in);
            if (roienv != NULL && strcmp(roienv, "CPUONLY") == 0) {
                ROI = CPU;
            }
            else if (roienv != NULL && strcmp(roienv, "PIMONLY") == 0) {
                ROI = PIM;
            }
            else {
                // if ROI == DEFAULT, we insert everything accordingly
                ROI = DEFAULT;
                assert(0);
            }
            std::string line;
            // skip the preceding lines
            while (std::getline(ifs, line)) {
                if (line.find("====") != std::string::npos) break;
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
                    else if (i == 2) {
                        ss >> token;
                        decision.parallel = (token == "O" ? 1 : 0);
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
            }
            if (ROI == CPU) {
                for (auto &func : M) {
                    if (func.getName() == "main") {
                        InjectOffloaderCall(M, func);
                    }
                }
            }
            errs() << "Found = " << found_cnt << ", Not Found = " << not_found_cnt << "\n";
            errs() << "Insert count = " << insert_cnt <<"\n";

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

static void loadPass(const PassManagerBuilder &,
                           legacy::PassManagerBase &PM) {
    PM.add(new OffloaderInjection());
}
static RegisterStandardPasses clangtoolLoader_Ox(PassManagerBuilder::EP_OptimizerLast, loadPass);
static RegisterStandardPasses clangtoolLoader_O0(PassManagerBuilder::EP_EnabledOnOptLevel0, loadPass);
