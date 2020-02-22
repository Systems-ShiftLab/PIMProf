#ifndef __PIMPROFANNOTATION_H__
#define __PIMPROFANNOTATION_H__

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "Common.h"

#define PIMProfMagicOP(op) ({        \
   unsigned long _op = (op); \
   __asm__ __volatile__ (                    \
   "\txchg %%rcx, %%rcx\n"                     \
   "\tmov %0, %%rax \n"             \
   "\tmov %1, %%rbx \n"           \
   "\tmov %2, %%rcx \n"           \
   :           /* output    */   \
   : "g"(0xffff),                              \
     "g"(0xffff),                             \
     "g"(_op)            /* input     */   \
   : "%rax", "%rbx", "%rcx", "memory"); /* clobbered */ \
})

static inline void PIMProfROIBegin() {
    // printf("PIMProf ROI begin\n");
    PIMProfMagicOP(MAGIC_OP_ROIBEGIN);
}

static inline void PIMProfROIEnd() {
    PIMProfMagicOP(MAGIC_OP_ROIEND);
    // printf("PIMProf ROI end\n");
}

static inline void PIMProfROIDecisionBegin() {
    printf("PIMProf ROI decision begin\n");
    PIMProfMagicOP(MAGIC_OP_ROIDECISIONBEGIN);
}

static inline void PIMProfROIDecisionEnd() {
    PIMProfMagicOP(MAGIC_OP_ROIDECISIONEND);
    printf("PIMProf ROI decision end\n");
}

#if defined PIMPROF
    #define PIMPROF_BEGIN_PROGRAM PIMProfROIDecisionBegin();
    #define PIMPROF_END_PROGRAM PIMProfROIDecisionEnd();
    #define PIMPROF_BEGIN_REG_PARALLEL PIMProfROIBegin();
    #define PIMPROF_END_REG_PARALLEL PIMProfROIEnd();
    #warning PIMPROF enabled
#endif

#endif // __PIMPROFANNOTATION_H__
