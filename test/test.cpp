#include <iostream>
#ifdef ZSIM
#include "zsimhooks.h"
#else
#include "PIMProfAnnotation.h"
#endif // PIMPROF
using namespace std;

void print4();
void pthreads_exec();

int main()
{
#ifdef ZSIM
    PIMPROF_BEGIN_PROGRAM
    #pragma omp for
#endif // ZSIM
    for (int i = 0; i < 5; i++) {
#ifdef ZSIM
    PIMPROF_BEGIN_REG_PARALLEL
#else
    PIMProfROIBegin();
#endif // ZSIM
        cout << "it is begin" << endl;
        int j = i % 5;
        switch(j) {
        case 0:
            cout << "it is 0" << endl; break;
        case 1:
            cout << "it is 1" << endl; break;
        case 2:
            cout << "it is 2" << endl; break;
        case 3:
            cout << "it is 3" << endl; break;
        case 4:
            print4(); 
            break;
        default:
            cout << "wtf" << endl;
        }
        cout << "it is end" << endl;
#ifdef ZSIM
    PIMPROF_END_REG_PARALLEL
#else
    PIMProfROIEnd();
#endif // ZSIM
    }
    // pthreads_exec();
#ifdef ZSIM
    PIMPROF_END_PROGRAM
#endif // ZSIM
    return 0;

}
