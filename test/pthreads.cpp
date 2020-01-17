#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#if defined ZSIM
#include "PIMProfZSimHooks.h"
#else
#include "PIMProfAnnotation.h"
#endif // ZSIM

#define NUM_THREADS 5

void *perform_work(void *arguments){

#if defined ZSIM
  PIMPROF_BEGIN_REG_PARALLEL
#else
  PIMProfROIBegin();
#endif // ZSIM
  int index = *((int *)arguments);
  int sleep_time = 1;
  printf("THREAD %d: Started.\n", index);
  printf("THREAD %d: Will be sleeping for %d seconds.\n", index, sleep_time);
  sleep(sleep_time);
  printf("THREAD %d: Ended.\n", index);
#if defined ZSIM
  PIMPROF_END_REG_PARALLEL
#else
  PIMProfROIEnd();
#endif // ZSIM
  return 0;
}

int pthreads_exec() {
  pthread_t threads[NUM_THREADS];
  int thread_args[NUM_THREADS];
  int i;
  int result_code;
  
  //create all threads one by one
  for (i = 0; i < NUM_THREADS; i++) {
    printf("IN MAIN: Creating thread %d.\n", i);
    thread_args[i] = i;
    result_code = pthread_create(&threads[i], NULL, perform_work, &thread_args[i]);
    assert(!result_code);
  }

  printf("IN MAIN: All threads are created.\n");

  //wait for each thread to complete
  for (i = 0; i < NUM_THREADS; i++) {
    result_code = pthread_join(threads[i], NULL);
    assert(!result_code);
    printf("IN MAIN: Thread %d has ended.\n", i);
  }

  printf("MAIN program has ended.\n");
  return 0;
}