#define _GNU_SOURCE 1
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <err.h>
#include <sched.h>  // sched_setaffinity
#include <unistd.h>
// #include "gem5/m5ops.h"
#include <ittnotify.h>
#include "Common.h"

int VTuneOffloader(int mode) {
    static __itt_domain *domain = NULL;
    if (domain == NULL) {
        domain = __itt_domain_create("pimprof.vtune_analysis");
        domain->flags = 1;
    }
    switch (mode) {
      case VTUNE_MODE_CREATE: // we have already done that by default
        break;
      case VTUNE_MODE_RESUME:
        __itt_resume(); break;
      case VTUNE_MODE_PAUSE:
        __itt_pause(); break;
      case VTUNE_MODE_DETACH:
        __itt_detach(); break;
      case VTUNE_MODE_FRAME_BEGIN:
        __itt_frame_begin_v3(domain, NULL); break;
      case VTUNE_MODE_FRAME_END:
        __itt_frame_end_v3(domain, NULL); break;
    }
    return 0;
}

// const unsigned PIMCoreIdBegin = 1;
// const unsigned PIMCoreIdEnd = 2;

// int PIMProfPreviousSite = -1;

// void print_affinity() {
//     cpu_set_t mask;
//     long nproc, i;

//     if (sched_getaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
//         perror("sched_getaffinity");
//         assert(0);
//     } else {
//         nproc = sysconf(_SC_NPROCESSORS_ONLN);
//         printf("sched_getaffinity = ");
//         for (i = 0; i < nproc; i++) {
//             printf("%d ", CPU_ISSET(i, &mask));
//         }
//         printf("\n");
//     }
// }

// int PIMProfOffloader(int decision, int mode, int bblid, int parallel)
// {
//     // printf("PIMProfOffloader: %d %d %d %d\n", decision, mode, bblid, parallel);
//     // if(mode==0){
//     //     m5_work_begin(decision,0);
//     // }else{
//     //     m5_work_end(decision,0);
//     // }
//     return 0;
// }

// int PIMProfOffloader2(int mode) {
//     return 0;
// }

// void PIMProfOffloader(int site, int bblid, double difference) {
//     if (site != PIMProfPreviousSite) {
//         PIMProfPreviousSite = site;
//         cpu_set_t mask;
//         int status;

//         CPU_ZERO(&mask);
//         if (site == 0) {
//             for (int i = 0; i < PIMCoreIdBegin; i++) {
//                 CPU_SET(i, &mask);
//             }
//         }
//         else if (site == 1) {
//             for (int i = PIMCoreIdBegin; i < PIMCoreIdEnd; i++) {
//                 CPU_SET(i, &mask);
//             }
//         }
//         else {
//             assert(0);
//         }
//         status = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
//         assert(status != -1);
//     }
//     printf("%s", (site == 0 ? "C" : "P"));
//     // print_affinity();
// }
