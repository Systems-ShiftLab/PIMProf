#include <iostream>
#if defined ZSIM
#include "PIMProfZSimHooks.h"
#endif
#if defined SNIPER
#include "PIMProfSniperHooks.h"
#endif
#if defined PIMPROF
#include "PIMProfAnnotation.h"
#endif
using namespace std;

void print4();
void pthreads_exec();
#define LEN 1
int a[LEN];

int main()
{
#if defined ZSIM || defined SNIPER
    PIMPROF_BEGIN_PROGRAM
#endif
#if defined PIMPROF
   PIMProfROIDecisionBegin();
#endif
    #pragma omp for
    for (int i = 0; i < LEN; i++) {
#if defined ZSIM || defined SNIPER
    PIMPROF_BEGIN_REG_PARALLEL
#endif
#if defined PIMPROF
    PIMProfROIBegin();
#endif
        // cout << "it is begin" << endl;
        // int j = i % 5;
        // switch(j) {
        // case 0:
        //     cout << "it is 0" << endl; break;
        // case 1:
        //     cout << "it is 1" << endl; break;
        // case 2:
        //     cout << "it is 2" << endl; break;
        // case 3:
        //     cout << "it is 3" << endl; break;
        // case 4:
        //     print4(); 
        //     break;
        // default:
        //     cout << "wtf" << endl;
        // }
        // cout << "it is end" << endl;
         /* Add 10 and 20 and store result into register %eax */
        // __asm__ __volatile__(
        //     "movl %%eax, %0;"
        //     : "=r" (a)
        // );
        // __asm__ __volatile__(
        //     "movl %%ebx, %0;"
        //     : "=r" (b)
        // );
        // __asm__ __volatile__ (
        //     "movl $10, %eax;"
        //     "movl $20, %ebx;"
        //     "addl %ebx, %eax;"
        // );
        // __asm__ __volatile__ (
        //     "movl %%eax, %0;"
        //    : "=r" (total)
        // );
        // __asm__ __volatile__(
        //     "movl %0, %%eax;"
        //     : : "r" (a)
        // );
        // __asm__ __volatile__(
        //     "movl %0, %%ebx;"
        //     : : "r" (b)
        // );
        a[i] = i;
        
#if defined ZSIM || defined SNIPER
    PIMPROF_END_REG_PARALLEL
#endif
#if defined PIMPROF
    PIMProfROIEnd();
#endif
    }
    // pthreads_exec();
#if defined ZSIM || defined SNIPER
    PIMPROF_END_PROGRAM
#endif
#if defined PIMPROF
    PIMProfROIDecisionEnd();
#endif
    return 0;

}
