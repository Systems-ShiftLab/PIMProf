#ifndef __PIMPROFANNOTATION_H__
#define __PIMPROFANNOTATION_H__

#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#define COMPILER_BARRIER() { __asm__ __volatile__("" ::: "memory");}

static inline void PIMProfMagicOP(uint64_t op) {
    COMPILER_BARRIER();
    __asm__ __volatile__("xchg %%rcx, %%rcx;" : : "c"(op));
    COMPILER_BARRIER();
}

#define MAGIC_OP_PIMPROFROIBEGIN (1)
#define MAGIC_OP_PIMPROFROIEND (2)
#define MAGIC_OP_PIMPROFROIDECISIONBEGIN (3)
#define MAGIC_OP_PIMPROFROIDECISIONEND (4)

static inline void PIMProfROIBegin() {
    printf("PIMProf ROI begin\n");
    PIMProfMagicOP(MAGIC_OP_PIMPROFROIBEGIN);
}

static inline void PIMProfROIEnd() {
    PIMProfMagicOP(MAGIC_OP_PIMPROFROIEND);
    printf("PIMProf ROI end\n");
}

static inline void PIMProfROIDecisionBegin() {
    printf("PIMProf ROI decision begin\n");
    PIMProfMagicOP(MAGIC_OP_PIMPROFROIDECISIONBEGIN);
}

static inline void PIMProfROIDecisionEnd() {
    PIMProfMagicOP(MAGIC_OP_PIMPROFROIDECISIONEND);
    printf("PIMProf ROI decision end\n");
}

#ifdef PIMPROF
    #define PIMPROF_BEGIN_PROGRAM PIMProfROIDecisionBegin();
    #define PIMPROF_END_PROGRAM PIMProfROIDecisionEnd();
    #define PIMPROF_BEGIN_REG_PARALLEL PIMProfROIBegin();
    #define PIMPROF_END_REG_PARALLEL PIMProfROIEnd();
    #warning PIMPROF enabled
#endif

#endif // __PIMPROFANNOTATION_H__