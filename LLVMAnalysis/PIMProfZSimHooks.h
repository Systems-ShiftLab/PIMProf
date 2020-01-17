#ifndef __PIMPROF_ZSIMHOOKS__
#define __PIMPROF_ZSIMHOOKS__

int PIMProfOffloader(int decision, int mode, int bblid, int parallel);
int PIMProfOffloaderNull(int decision, int mode, int bblid, int parallel);
int PIMProfOffloader2(int mode);

#include <stdint.h>
#include <stdio.h>
#include <assert.h>

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
    // printf("[" HOOKS_STR "] PIM begin\n");
    zsim_magic_op(ZSIM_MAGIC_OP_FUNCTION_BEGIN);
}

// LOIS
static inline void zsim_PIM_function_end() {
    zsim_magic_op(ZSIM_MAGIC_OP_FUNCTION_END);
    // printf("[" HOOKS_STR "] PIM end\n");
}

static inline void zsim_heartbeat() {
    zsim_magic_op(ZSIM_MAGIC_OP_HEARTBEAT);
}

static inline void zsim_work_begin() { zsim_magic_op(ZSIM_MAGIC_OP_WORK_BEGIN); }
static inline void zsim_work_end() { zsim_magic_op(ZSIM_MAGIC_OP_WORK_END); }


#if ZSIM == 0
    #define PIMPROF_BEGIN_PROGRAM zsim_roi_begin(); zsim_PIM_function_begin();
    #define PIMPROF_END_PROGRAM zsim_PIM_function_end(); zsim_roi_end();
    #define PIMPROF_BEGIN_REG_PARALLEL zsim_PIM_function_end();
    #define PIMPROF_END_REG_PARALLEL zsim_PIM_function_begin();
    #warning ZSIM == 0
#elif ZSIM == 1
    #define PIMPROF_BEGIN_PROGRAM zsim_roi_begin();
    #define PIMPROF_END_PROGRAM zsim_roi_end();
    #define PIMPROF_BEGIN_REG_PARALLEL zsim_PIM_function_begin();
    #define PIMPROF_END_REG_PARALLEL zsim_PIM_function_end();
    #warning ZSIM == 1
#elif ZSIM == 2
    #define PIMPROF_BEGIN_PROGRAM zsim_roi_begin(); zsim_PIM_function_begin();
    #define PIMPROF_END_PROGRAM zsim_roi_end(); zsim_PIM_function_end();
    #define PIMPROF_BEGIN_REG_PARALLEL ;
    #define PIMPROF_END_REG_PARALLEL ;
    #warning ZSIM == 2
#elif ZSIM == 3
    #define PIMPROF_BEGIN_PROGRAM zsim_roi_begin();
    #define PIMPROF_END_PROGRAM zsim_roi_end();
    #define PIMPROF_BEGIN_REG_PARALLEL zsim_roi_end();
    #define PIMPROF_END_REG_PARALLEL zsim_roi_begin();
    #warning ZSIM == 3
#else
#endif

#endif // __PIMPROF_ZSIMHOOKS__