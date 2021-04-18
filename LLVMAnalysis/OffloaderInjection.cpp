//===- OffloaderInjection.cpp - Pass that injects calls for PIM offloading --*- C++ -*-===//
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

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"


#include <iostream>
#include <assert.h>
#include <fstream>
#include <sstream>

#include "MurmurHash3.h"
#include "Common.h"
#include "InjectMagic.h"

using namespace llvm;

namespace PIMProf { // Anonymous namespace

struct Decision {
    CostSite decision;
    int bblid;
    double difference;
    int parallel;
};

class DecisionMap {
  private:
    UUIDHashMap<Decision> decision_map;
  public:
    Decision getBasicBlockDecision(Module &M, BasicBlock &BB) {
        Decision decision;
        decision.decision = CostSite::INVALID;
        // use the content of BB itself as the hash key
        std::string BB_content;
        raw_string_ostream rso(BB_content);
        rso << BB;
        uint64_t bblhash[2];

        MurmurHash3_x64_128(BB_content.c_str(), BB_content.size(), 0, bblhash);

        // for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
        //     (*i).print(errs());
        //     errs() << "\n";
        // }
        // errs() << "\n";
        // errs() << "Hash = " << bblhash[1] << " " << bblhash[0] << "\n";

        UUID uuid(bblhash[1], bblhash[0]);
        if (decision_map.find(uuid) == decision_map.end()) {
        }
        else {
            // std::cerr << "found same function." << std::endl;
            decision = decision_map[uuid];
        }

        return decision;

        // // divide all parameters into uint64_t, because this is what pin supports
        // Value *hi = ConstantInt::get(
        //     IntegerType::get(M.getContext(), 64), bblhash[1]);
        // Value *lo = ConstantInt::get(
        //     IntegerType::get(M.getContext(), 64), bblhash[0]);


        // std::string funcname = BB.getParent()->getName();
        // uint64_t isomp = (funcname.find(OpenMPIdentifier) != std::string::npos);
        // // std::cout << isomp << " " << ControlValue::GetControlValue(MAGIC_OP_ANNOTATIONHEAD, isomp) << std::endl;
    }

    Decision getMainDecision() {
        return decision_map[UUID(MAIN_BBLID, MAIN_BBLID)];
    }

    void initDecisionMap(const char *in)
    {
        std::ifstream ifs(in, std::ifstream::in);
        /********************************************************
         * Parser for PIMProf output file
        ********************************************************/
        std::string line;
        // skip the preceding lines
        while (std::getline(ifs, line)) {
            if (line.find(HORIZONTAL_LINE) != std::string::npos) break;
        }

        while(std::getline(ifs, line)) {
            std::stringstream ss(line);
            std::string token;
            Decision decision;
            int64_t hi, lo;
            for (int i = 0; i < 10; i++) {
                if (i == 0) {
                    ss >> decision.bblid;
                }
                else if (i == 1) {
                    ss >> token;
                    decision.decision = (token == "P" ? CostSite::PIM : CostSite::CPU);
                }
                else if (i == 5) {
                    ss >> decision.difference;
                }
                else if (i == 6) {
                    ss >> hi;
                }
                else if (i == 7) {
                    ss >> lo;
                }
                else {
                    // ignore it
                    ss >> token;
                }
            }
            decision_map[UUID(hi, lo)] = decision;
        }
        ifs.close();
    }

};


int found_cnt = 0, not_found_cnt = 0;
int cpu_inject_cnt = 0, pim_inject_cnt = 0;

// ROI indicates which part we want to annotate in the program
// CostSite ROI = CostSite::INVALID;
InjectMode Mode = InjectMode::INVALID;
CostSite MainDecision = CostSite::INVALID;

DecisionMap decision_map;

/********************************************************
* Sniper
********************************************************/

void InjectSniperOffloaderCall(Module &M, BasicBlock &BB) {
    Decision decision = decision_map.getBasicBlockDecision(M, BB);

    // if not found, then return
    if (decision.decision == CostSite::INVALID) {
        not_found_cnt++;
        return;
    }
    found_cnt++;

    // TODO: For testing purpose
    std::string BB_content;
    raw_string_ostream rso(BB_content);
    rso << BB;
    uint64_t bblhash[2];
    MurmurHash3_x64_128(BB_content.c_str(), BB_content.size(), 0, bblhash);

    // need to skip all PHIs and LandingPad instructions
    // check the declaration of getFirstInsertionPt()
    Instruction *beginning = &(*BB.getFirstInsertionPt());

    if (decision.decision == CostSite::PIM) {
        InjectSimMagic2(M, SNIPER_SIM_PIMPROF_OFFLOAD_START, bblhash[1], PIMPROF_DECISION_PIM, beginning);
        InjectSimMagic2(M, SNIPER_SIM_PIMPROF_OFFLOAD_END, bblhash[1], PIMPROF_DECISION_PIM, BB.getTerminator());
        pim_inject_cnt++;
    }
    else {
        InjectSimMagic2(M, SNIPER_SIM_PIMPROF_OFFLOAD_START, bblhash[1], PIMPROF_DECISION_CPU, beginning);
        InjectSimMagic2(M, SNIPER_SIM_PIMPROF_OFFLOAD_END, bblhash[1], PIMPROF_DECISION_CPU, BB.getTerminator());
        cpu_inject_cnt++;
    }
}

void InjectSniperOffloaderCall(Module &M, Function &F) {
    // need to skip all PHIs and LandingPad instructions
    // check the declaration of getFirstInsertionPt()
    Instruction *beginning = &(*F.getEntryBlock().getFirstInsertionPt());
    InjectSimMagic0(M, SNIPER_SIM_CMD_ROI_START, beginning);

    if (MainDecision == CostSite::PIM) {
        // offload start
        // prototype: OffloadStart(uint64_t hi, uint64_t type)
        InjectSimMagic2(M, SNIPER_SIM_PIMPROF_OFFLOAD_START, MAIN_BBLID, PIMPROF_DECISION_PIM, beginning);
    }
    else {
        InjectSimMagic2(M, SNIPER_SIM_PIMPROF_BBL_START, MAIN_BBLID, PIMPROF_DECISION_CPU, beginning);
    }

    // inject an end call before every return instruction
    for (auto &BB : F) {
        for (auto &I : BB) {
            if (isa<ReturnInst>(I)) {
                if (MainDecision == CostSite::PIM) {
                    // offload end
                    // prototype: OffloadStart(uint64_t hi, uint64_t type)
                    // here type=PIMPROF_TEST_END shows this is the end of main BBL
                    InjectSimMagic2(M, SNIPER_SIM_PIMPROF_OFFLOAD_END, MAIN_BBLID, PIMPROF_DECISION_PIM, &I);
                }
                else {
                    InjectSimMagic2(M, SNIPER_SIM_PIMPROF_BBL_END, MAIN_BBLID, PIMPROF_DECISION_CPU, &I);
                }
                InjectSimMagic0(M, SNIPER_SIM_CMD_ROI_END, &I);
            }
        }
    }
}

/********************************************************
* VTune
********************************************************/

// void InjectVTuneOffloaderCall(Module &M, BasicBlock &BB) {
//     Decision decision = decision_map.getBasicBlockDecision(M, BB);

//     // if not found, then return
//     if (decision.decision == CostSite::INVALID) {
//         not_found_cnt++;
//         return;
//     }
//     found_cnt++;

//     // need to skip all PHIs and LandingPad instructions
//     // check the declaration of getFirstInsertionPt()
//     Instruction *beginning = &(*BB.getFirstInsertionPt());
//     if (decision.decision == CostSite::CPU) {
//         if (ROI == CostSite::CPU || ROI == CostSite::ALL) {
//             InjectVTuneITT(M, VTUNE_MODE_FRAME_BEGIN, beginning);
//             InjectVTuneITT(M, VTUNE_MODE_FRAME_END, BB.getTerminator());
//             cpu_inject_cnt++;
//         }
//     }
//     if (decision.decision == CostSite::PIM) {
//         if (ROI == CostSite::NOTPIM) {
//             InjectVTuneITT(M, VTUNE_MODE_FRAME_END, beginning);
//             InjectVTuneITT(M, VTUNE_MODE_FRAME_BEGIN, BB.getTerminator());
//             pim_inject_cnt++;
//         }
//         if (ROI == CostSite::PIM || ROI == CostSite::ALL) {
//             InjectVTuneITT(M, VTUNE_MODE_FRAME_BEGIN, beginning);
//             InjectVTuneITT(M, VTUNE_MODE_FRAME_END, BB.getTerminator());
//             pim_inject_cnt++;
//         }
//     }
// }

// void InjectVTuneOffloaderCall(Module &M, Function &F) {
//     // need to skip all PHIs and LandingPad instructions
//     // check the declaration of getFirstInsertionPt()
//     Instruction *beginning = &(*F.getEntryBlock().getFirstInsertionPt());
//     InjectVTuneITT(M, VTUNE_MODE_RESUME, beginning);
//     if (ROI == CostSite::NOTPIM) {
//         InjectVTuneITT(M, VTUNE_MODE_FRAME_BEGIN, beginning);
//     }

//     // inject an end call before every return instruction
//     for (auto &BB : F) {
//         for (auto &I : BB) {
//             if (isa<ReturnInst>(I)) {
//                 if (ROI == CostSite::NOTPIM) {
//                     InjectVTuneITT(M, VTUNE_MODE_FRAME_END, &I);
//                 }
//                 InjectVTuneITT(M, VTUNE_MODE_DETACH, &I);
//             }
//         }
//     }
// }

struct OffloaderInjection : public ModulePass {
    static char ID;
    OffloaderInjection() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M) {
        char *decisionfileenv = NULL;
        // char *roienv = NULL;
        char *injectmodeenv = NULL;

        decisionfileenv = std::getenv(PIMProfDecisionEnv.c_str());
        // roienv = std::getenv(PIMProfROIEnv.c_str());
        injectmodeenv = std::getenv(PIMProfInjectModeEnv.c_str());

        assert(decisionfileenv != NULL);
        // assert(roienv != NULL);
        assert(injectmodeenv != NULL);

        std::cout << "filename: " << decisionfileenv << std::endl;
        // std::cout << "roi: " << roienv << std::endl;
        // std::cout << "inject mode:" << injectmodeenv << std::endl;

        // if (strcmp(roienv, "ALL") == 0) ROI = CostSite::ALL;
        // else if (strcmp(roienv, "CPUONLY") == 0) ROI = CostSite::CPU;
        // else if (strcmp(roienv, "PIMONLY") == 0) ROI = CostSite::PIM;
        // else {
        //     assert(0 && "Invalid environment variable PIMPROFROI");
        // }

        if (strcmp(injectmodeenv, "SNIPER") == 0) Mode = InjectMode::SNIPER;
        // else if (strcmp(injectmodeenv, "VTUNE") == 0) Mode = InjectMode::VTUNE;
        else if (strcmp(injectmodeenv, "PIMPROF") == 0) Mode = InjectMode::PIMPROF;
        else {
            assert(0 && "Invalid environment variable PIMPROFINJECTMODE");
        }

        decision_map.initDecisionMap(decisionfileenv);

        MainDecision = decision_map.getMainDecision().decision;

        /********************************************************
         * Inject offloader function to each basic block according to
         * their basic block ID and corresponding decision.
         * Simply assume that the input program is the same as the input
         * in annotator injection pass
        ********************************************************/
        if (Mode == InjectMode::SNIPER) {
            for (auto &func : M) {
                for (auto &bb: func) {
                    // errs() << "Before offloading: " << bb.getName() << "\n";
                    // for (auto i = bb.begin(), ie = bb.end(); i != ie; i++) {
                    //     (*i).print(errs());
                    //     errs() << "\n";
                    // }
                    // errs() << "\n";
                    InjectSniperOffloaderCall(M, bb);
                }
            }
            for (auto &func : M) {
                if (func.getName() == "main") {
                    InjectSniperOffloaderCall(M, func);
                }
            }
        }
        if (Mode == InjectMode::VTUNE) {
            assert(0);
            // for (auto &func : M) {
            //     for (auto &bb: func) {
            //         InjectVTuneOffloaderCall(M, bb);
            //     }
            // }
            // for (auto &func : M) {
            //     if (func.getName() == "main") {
            //         InjectVTuneOffloaderCall(M, func);
            //     }
            // }
        }

        errs() << "Found = " << found_cnt << ", Not Found = " << not_found_cnt << "\n";
        errs() << "CPU inject count = " << cpu_inject_cnt << ", PIM inject count = " << pim_inject_cnt << "\n";
        
        outs() << "Found = " << found_cnt << ", Not Found = " << not_found_cnt << "\n";
        outs() << "CPU inject count = " << cpu_inject_cnt << ", PIM inject count = " << pim_inject_cnt << "\n";

        PIMProfAAW aaw = PIMProfAAW();
        for (auto &func: M) {
            func.print(outs(), &aaw);
        }

        // M.print(errs(), nullptr);
        return true;
    }
};

} // namespace PIMProf

char PIMProf::OffloaderInjection::ID = 0;
static RegisterPass<PIMProf::OffloaderInjection> RegisterMyPass(
    "OffloaderInjection", "Inject offloader when switching between CPU and PIM is required.");

static void loadPass(const PassManagerBuilder &,
                           legacy::PassManagerBase &PM) {
    PM.add(new PIMProf::OffloaderInjection());
}
static RegisterStandardPasses clangtoolLoader_Ox(PassManagerBuilder::EP_OptimizerLast, loadPass);
static RegisterStandardPasses clangtoolLoader_O0(PassManagerBuilder::EP_EnabledOnOptLevel0, loadPass);
