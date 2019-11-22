#define _GNU_SOURCE 1
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <err.h>
#include <sched.h>  // sched_setaffinity
#include <unistd.h>

const unsigned PIMCoreIdBegin = 1;
const unsigned PIMCoreIdEnd = 2;

int PIMProfPreviousSite = -1;

void print_affinity() {
    cpu_set_t mask;
    long nproc, i;

    if (sched_getaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
        perror("sched_getaffinity");
        assert(0);
    } else {
        nproc = sysconf(_SC_NPROCESSORS_ONLN);
        printf("sched_getaffinity = ");
        for (i = 0; i < nproc; i++) {
            printf("%d ", CPU_ISSET(i, &mask));
        }
        printf("\n");
    }
}

int PIMProfOffloader(int decision, int mode) 
{
    printf("PIMProfOffloader: %d %d\n", decision, mode);
    return 0;
}

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