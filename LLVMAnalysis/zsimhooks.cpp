#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <stack>

//Avoid optimizing compilers moving code around this barrier
#define COMPILER_BARRIER() { __asm__ __volatile__("" ::: "memory");}

//These need to be in sync with the simulator
#define ZSIM_MAGIC_OP_ROI_BEGIN         (1025)
#define ZSIM_MAGIC_OP_ROI_END           (1026)
#define ZSIM_MAGIC_OP_REGISTER_THREAD   (1027)
#define ZSIM_MAGIC_OP_HEARTBEAT         (1028)
#define ZSIM_MAGIC_OP_WORK_BEGIN        (1029) //ubik
#define ZSIM_MAGIC_OP_WORK_END          (1030) //ubik

#define ZSIM_MAGIC_OP_FUNCTION_BEGIN    (1031) // LOIS
#define ZSIM_MAGIC_OP_FUNCTION_END      (1032) // LOIS

#ifdef __x86_64__
#define HOOKS_STR  "HOOKS"
static inline void zsim_magic_op(uint64_t op) {
    COMPILER_BARRIER();
    __asm__ __volatile__("xchg %%rcx, %%rcx;" : : "c"(op));
    COMPILER_BARRIER();
}
#else
#define HOOKS_STR  "NOP-HOOKS"
static inline void zsim_magic_op(uint64_t op) {
    //NOP
}
#endif

static inline void zsim_roi_begin() {
    printf("[" HOOKS_STR "] ROI begin\n");
    zsim_magic_op(ZSIM_MAGIC_OP_ROI_BEGIN);
}

static inline void zsim_roi_end() {
    zsim_magic_op(ZSIM_MAGIC_OP_ROI_END);
    printf("[" HOOKS_STR  "] ROI end\n");
}

// LOIS
static inline void zsim_PIM_function_begin() {
    zsim_magic_op(ZSIM_MAGIC_OP_FUNCTION_BEGIN);
}

// LOIS
static inline void zsim_PIM_function_end() {
    zsim_magic_op(ZSIM_MAGIC_OP_FUNCTION_END);
}

static inline void zsim_heartbeat() {
    zsim_magic_op(ZSIM_MAGIC_OP_HEARTBEAT);
}

static inline void zsim_work_begin() { zsim_magic_op(ZSIM_MAGIC_OP_WORK_BEGIN); }
static inline void zsim_work_end() { zsim_magic_op(ZSIM_MAGIC_OP_WORK_END); }

bool scopeInitialized = false;
std::stack<int> scope;

int PIMProfOffloader(int decision, int mode, int bblid, int parallel)
{
    if (!scopeInitialized) {
        scopeInitialized = true;
        scope.push(-1); // push an invalid element as the global scope
    }

    if (mode == 0) {
        if (scope.top() == 1 && decision == 0) {
            zsim_PIM_function_end();
        }
        else if (scope.top() != decision && decision == 1) {
            zsim_PIM_function_begin();
        }
        else {
            assert(0);
        }
        scope.push(decision);
    }
    if (mode == 1) {
        scope.pop();
        if (scope.top() == 1 && decision == 0) {
            zsim_PIM_function_begin();
        }
        else if (scope.top() != decision && decision == 1) {
            zsim_PIM_function_end();
        }
        else {
            assert(0);
        }
    }
    return 0;
}

int PIMProfOffloaderNull(int decision, int mode, int bblid, int parallel)
{
    return 0;
}

int PIMProfOffloader2(int mode) {
    if (mode == 0) {
        zsim_roi_begin();
    }
    else if (mode == 1) {
        zsim_roi_end();
    }
    else {
        assert(0);
    }
    return 0;
}