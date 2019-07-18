#define _GNU_SOURCE 1
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <err.h>
#include <sched.h>  // sched_setaffinity
#include <unistd.h>

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

void cpu_set(unsigned cpu) {
    cpu_set_t mask;
    int status;

    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    status = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
    print_affinity();
}

void PIMProfOffloader() {
    
}