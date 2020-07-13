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

namespace { // Anonymous namespace

/*
    Meaning of different mode:
    "CPU" or "CPUONLY" - Put start and end annotations around CPU-friendly BBLs
    "PIM" or "PIMONLY" - Put start and end annotations around PIM-friendly BBLs
    "NOTPIM" - Put end annotations at the start of PIM-friendly BBLs, and put start annotations at the end of PIM-friendly BBLs
    "ALL" - Put start and end annotations at all BBLs, in this case the second argument can be used to differentiate whether it is CPU or PIM friendly
*/
enum class CallSite {
    CPU, PIM, MAX_COST_SITE,
    NOTPIM, ALL,
    DEFAULT = 0x0fffffff,
    INVALID = 0x3fffffff // a placeholder that does not count as a cost site
};

enum class InjectMode {
    SNIPER, VTUNE, PIMPROF,
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

class DecisionMap {
  private:
    std::unordered_map<UUID, Decision, HashFunc> decision_map;
  public:
    Decision getBasicBlockDecision(Module &M, BasicBlock &BB) {
        Decision decision;
        decision.decision = CallSite::INVALID;
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

    void initDecisionMap(const char *in)
    {
        std::ifstream ifs(in, std::ifstream::in);
        /********************************************************
         * Parser for PIMProf output file
        ********************************************************/
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
                    decision.decision = (token == "P" ? CallSite::PIM : CallSite::CPU);
                }
                else if (i == 2) {
                    ss >> token;
                    decision.parallel = (token == "O" ? true : false);
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
    }

};


int found_cnt = 0, not_found_cnt = 0;
int cpu_inject_cnt = 0, pim_inject_cnt = 0;

CallSite ROI = CallSite::INVALID;
InjectMode Mode = InjectMode::INVALID;

DecisionMap decision_map;


/********************************************************
* Injection function for Sniper (inject asm)
********************************************************/

// Inject Sniper's SimMagic0 before the LLVM::Instruction pointed by injectPt
void InjectSimMagic0(Module &M, uint64_t arg0, Instruction *insertPt)
{
    LLVMContext &ctx = M.getContext();
    std::vector<Type *> argtype {
        Type::getInt64Ty(ctx), Type::getInt64Ty(ctx), Type::getInt64Ty(ctx)
    };
    FunctionType *ty = FunctionType::get(
        Type::getVoidTy(ctx), argtype, false
    );
    // template of Sniper's SimMagic0
    InlineAsm *ia = InlineAsm::get(
        ty,
        "\tmov $0, %rax \n"
        "\txchg %bx, %bx\n",
        "imr,~{rax},~{dirflag},~{fpsr},~{flags}",
        true
    );
    Value *val0 = ConstantInt::get(IntegerType::get(M.getContext(), 64), arg0);
    std::vector<Value *> arglist {val0};
    CallInst::Create(
            ia, arglist, "", insertPt);
}

void InjectSimMagic2(Module &M, uint64_t arg0, uint64_t arg1, uint64_t arg2, Instruction *insertPt)
{
    LLVMContext &ctx = M.getContext();
    std::vector<Type *> argtype {
        Type::getInt64Ty(ctx), Type::getInt64Ty(ctx), Type::getInt64Ty(ctx)
    };
    FunctionType *ty = FunctionType::get(
        Type::getVoidTy(ctx), argtype, false
    );
    // template of Sniper's SimMagic0
    InlineAsm *ia = InlineAsm::get(
        ty,
        "\tmov $0, %rax \n"
        "\tmov $1, %rbx \n"
        "\tmov $2, %rcx \n"
        "\txchg %bx, %bx\n",
        "imr,imr,imr,~{rax},~{rbx},~{rcx},~{dirflag},~{fpsr},~{flags}",
        true
    );
    Value *val0 = ConstantInt::get(IntegerType::get(ctx, 64), arg0);
    Value *val1 = ConstantInt::get(IntegerType::get(ctx, 64), arg1);
    Value *val2 = ConstantInt::get(IntegerType::get(ctx, 64), arg2);
    std::vector<Value *> arglist {val0, val1, val2};
    CallInst::Create(
            ia, arglist, "", insertPt);
}

void InjectVTuneITT(Module &M, VTUNE_MODE mode, Instruction *insertPt) {
    LLVMContext &ctx = M.getContext();
    Function *offloader = dyn_cast<Function>(
        M.getOrInsertFunction(
            VTuneOffloaderName, 
            FunctionType::getInt32Ty(ctx),
            Type::getInt32Ty(ctx)
        )
    );
    Value *val0 = ConstantInt::get(IntegerType::get(M.getContext(), 32), mode);
    std::vector<Value *> arglist {val0};
    CallInst::Create(
        offloader, arglist, "", insertPt);
}

void InjectSniperOffloaderCall(Module &M, BasicBlock &BB) {
    Decision decision = decision_map.getBasicBlockDecision(M, BB);

    // if not found, then return
    if (decision.decision == CallSite::INVALID) {
        not_found_cnt++;
        return;
    }
    found_cnt++;

    // need to skip all PHIs and LandingPad instructions
    // check the declaration of getFirstInsertionPt()
    Instruction *beginning = &(*BB.getFirstInsertionPt());
    if (decision.decision == CallSite::CPU) {
        if (ROI == CallSite::CPU || ROI == CallSite::ALL) {
            InjectSimMagic2(M, SIM_PIM_OFFLOAD_START, decision.bblid, (uint64_t) CallSite::CPU, beginning);
            InjectSimMagic2(M, SIM_PIM_OFFLOAD_END, decision.bblid, (uint64_t) CallSite::CPU, BB.getTerminator());
            cpu_inject_cnt++;
        }
    }
    if (decision.decision == CallSite::PIM) {
        if (ROI == CallSite::NOTPIM) {
            InjectSimMagic2(M, SIM_PIM_OFFLOAD_END, decision.bblid, (uint64_t) CallSite::CPU, beginning);
            InjectSimMagic2(M, SIM_PIM_OFFLOAD_START, decision.bblid, (uint64_t) CallSite::CPU, BB.getTerminator());
            pim_inject_cnt++;
        }
        if (ROI == CallSite::PIM || ROI == CallSite::ALL) {
            InjectSimMagic2(M, SIM_PIM_OFFLOAD_START, decision.bblid, (uint64_t) CallSite::PIM, beginning);
            InjectSimMagic2(M, SIM_PIM_OFFLOAD_END, decision.bblid, (uint64_t) CallSite::PIM, BB.getTerminator());
            pim_inject_cnt++;
        }
    }
}

void InjectSniperOffloaderCall(Module &M, Function &F) {
    // need to skip all PHIs and LandingPad instructions
    // check the declaration of getFirstInsertionPt()
    Instruction *beginning = &(*F.getEntryBlock().getFirstInsertionPt());
    InjectSimMagic0(M, SIM_CMD_ROI_START, beginning);
    if (ROI == CallSite::NOTPIM) {
        InjectSimMagic2(M, SIM_PIM_OFFLOAD_START, -1, (uint64_t) CallSite::CPU, beginning);
    }

    // inject an end call before every return instruction
    for (auto &BB : F) {
        for (auto &I : BB) {
            if (isa<ReturnInst>(I)) {
                if (ROI == CallSite::NOTPIM) {
                    InjectSimMagic2(M, SIM_PIM_OFFLOAD_END, -1, (uint64_t) CallSite::CPU, &I);
                }
                InjectSimMagic0(M, SIM_CMD_ROI_END, &I);
            }
        }
    }
}

/********************************************************
* Injection function for VTune (inject function call)
********************************************************/

void InjectVTuneOffloaderCall(Module &M, BasicBlock &BB) {
    Decision decision = decision_map.getBasicBlockDecision(M, BB);

    // if not found, then return
    if (decision.decision == CallSite::INVALID) {
        not_found_cnt++;
        return;
    }
    found_cnt++;

    // need to skip all PHIs and LandingPad instructions
    // check the declaration of getFirstInsertionPt()
    Instruction *beginning = &(*BB.getFirstInsertionPt());
    if (decision.decision == CallSite::CPU) {
        if (ROI == CallSite::CPU || ROI == CallSite::ALL) {
            InjectVTuneITT(M, VTUNE_MODE_FRAME_BEGIN, beginning);
            InjectVTuneITT(M, VTUNE_MODE_FRAME_END, BB.getTerminator());
            cpu_inject_cnt++;
        }
    }
    if (decision.decision == CallSite::PIM) {
        if (ROI == CallSite::NOTPIM) {
            InjectVTuneITT(M, VTUNE_MODE_FRAME_END, beginning);
            InjectVTuneITT(M, VTUNE_MODE_FRAME_BEGIN, BB.getTerminator());
            pim_inject_cnt++;
        }
        if (ROI == CallSite::PIM || ROI == CallSite::ALL) {
            InjectVTuneITT(M, VTUNE_MODE_FRAME_BEGIN, beginning);
            InjectVTuneITT(M, VTUNE_MODE_FRAME_END, BB.getTerminator());
            pim_inject_cnt++;
        }
    }
}

void InjectVTuneOffloaderCall(Module &M, Function &F) {
    // need to skip all PHIs and LandingPad instructions
    // check the declaration of getFirstInsertionPt()
    Instruction *beginning = &(*F.getEntryBlock().getFirstInsertionPt());
    InjectVTuneITT(M, VTUNE_MODE_RESUME, beginning);
    if (ROI == CallSite::NOTPIM) {
        InjectVTuneITT(M, VTUNE_MODE_FRAME_BEGIN, beginning);
    }

    // inject an end call before every return instruction
    for (auto &BB : F) {
        for (auto &I : BB) {
            if (isa<ReturnInst>(I)) {
                if (ROI == CallSite::NOTPIM) {
                    InjectVTuneITT(M, VTUNE_MODE_FRAME_END, &I);
                }
                InjectVTuneITT(M, VTUNE_MODE_DETACH, &I);
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
        char *injectmodeenv = NULL;

        decisionfileenv = std::getenv(PIMProfDecisionEnv.c_str());
        roienv = std::getenv(PIMProfROIEnv.c_str());
        injectmodeenv = std::getenv(PIMProfInjectModeEnv.c_str());

        assert(decisionfileenv != NULL);
        assert(roienv != NULL);
        assert(injectmodeenv != NULL);

        std::cout << "filename: " << decisionfileenv << std::endl;
        std::cout << "roi: " << roienv << std::endl;
        std::cout << "inject mode:" << injectmodeenv << std::endl;

        if (strcmp(roienv, "ALL") == 0) ROI = CallSite::ALL;
        else if (strcmp(roienv, "NOTPIM") == 0) ROI = CallSite::NOTPIM;
        else if (strcmp(roienv, "CPUONLY") == 0) ROI = CallSite::CPU;
        else if (strcmp(roienv, "PIMONLY") == 0) ROI = CallSite::PIM;
        else {
            assert(0 && "Invalid environment variable PIMPROFROI");
        }

        if (strcmp(injectmodeenv, "SNIPER") == 0) Mode = InjectMode::SNIPER;
        else if (strcmp(injectmodeenv, "VTUNE") == 0) Mode = InjectMode::VTUNE;
        else if (strcmp(injectmodeenv, "PIMPROF") == 0) Mode = InjectMode::PIMPROF;
        else {
            assert(0 && "Invalid environment variable PIMPROFINJECTMODE");
        }

        decision_map.initDecisionMap(decisionfileenv);

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
            for (auto &func : M) {
                for (auto &bb: func) {
                    InjectVTuneOffloaderCall(M, bb);
                }
            }
            for (auto &func : M) {
                if (func.getName() == "main") {
                    InjectVTuneOffloaderCall(M, func);
                }
            }
        }
        
        errs() << "Found = " << found_cnt << ", Not Found = " << not_found_cnt << "\n";
        errs() << "CPU inject count = " << cpu_inject_cnt << ", PIM inject count = " << pim_inject_cnt << "\n";

        PIMProfAAW aaw = PIMProfAAW();
        for (auto &func: M) {
            func.print(outs(), &aaw);
        }

        // M.print(errs(), nullptr);
        return true;
    }
};

} // Anonymous namespace

char OffloaderInjection::ID = 0;
static RegisterPass<OffloaderInjection> RegisterMyPass(
    "OffloaderInjection", "Inject offloader when switching between CPU and PIM is required.");

static void loadPass(const PassManagerBuilder &,
                           legacy::PassManagerBase &PM) {
    PM.add(new OffloaderInjection());
}
static RegisterStandardPasses clangtoolLoader_Ox(PassManagerBuilder::EP_OptimizerLast, loadPass);
static RegisterStandardPasses clangtoolLoader_O0(PassManagerBuilder::EP_EnabledOnOptLevel0, loadPass);
