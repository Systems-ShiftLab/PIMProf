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

#ifndef NUM1
#define NUM1 9
#endif
#ifndef NUM2
#define NUM2 9
#endif
#ifndef ITER1
#define ITER1 3
#endif

#ifndef RATE
#define RATE 50
#endif
#ifndef RANDOM
#define RANDOM 100
#endif
#ifndef SEQUENTIAL
#define SEQUENTIAL 1
#endif

#include <cassert>

using namespace std;

void print4();
void pthreads_exec();

const int PAGE_SIZE = 0x1000;
int INTNUM_IN_PAGE;
int INTNUM_IN_LINE;
const int LINE_SIZE = 0x40;
const int PAGE_MAX = 0x10000;
const unsigned int A = 1103515245;
const unsigned int C = 12345;
const unsigned int M = 0x7fffffff;
unsigned int *arr;

// #ifndef ITER2
// #define ITER2 1
// #endif

#define ABS(x) ((x) < 0 ? -(x) : (x))

double **matrix_alloc(int size)
{
    double **ptr = (double **)malloc(sizeof(double *) * size);
    int i;
    for (i = 0; i < size; i++)
    {
        ptr[i] = (double *)malloc(sizeof(double) * size);
    }
    return ptr;
}

void matrix_free(double **ptr, int size)
{
    int i;
    for (i = 0; i < size; i++)
        free(ptr[i]);
    free(ptr);
}

void matrix_print(double **ptr, int size)
{
    int i, j;
    for (i = 0; i < size; i++)
    {
        for (j = 0; j < size; j++)
            printf("%lf\t", ptr[i][j]);
        printf("\n");
    }
}

void transpose(double **arr, int size)
{
    int i, j;
    for (i = 0; i < size; i++)
    {
        for (j = 0; j < i; j++)
        {
            double temp = arr[i][j];
            arr[i][j] = arr[j][i];
            arr[j][i] = temp;
        }
    }
}

// void inverse(double **arr, int size)
// {
//     int p[size];
//     int i, j, k;
//     for (i = 0; i < size; i++)
//     {
//         p[i] = i;
//     }
//     // in-place LUP decomposition
//     double **lu = matrix_alloc(size);
//     for (i = 0; i < size; i++)
//     {
//         for (j = 0; j < size; j++)
//             lu[i][j] = arr[i][j];
//     }

//     for (k = 0; k < size; k++)
//     {
//         double maxabs = 0;
//         int maxidx = 0;
//         for (i = k; i < size; i++)
//         {
//             if (ABS(lu[i][k]) > maxabs)
//             {
//                 maxabs = ABS(lu[i][k]);
//                 maxidx = i;
//             }
//         }
//         if (maxabs == 0)
//         {
//             assert(0 && "singular matrix\n");
//         }
//         int temp = p[k];
//         p[k] = p[maxidx];
//         p[maxidx] = temp;
//         for (i = 0; i < size; i++)
//         {
//             double dtemp = lu[k][i];
//             lu[k][i] = lu[maxidx][i];
//             lu[maxidx][i] = dtemp;
//         }
//         for (i = k + 1; i < size; i++)
//         {
//             lu[i][k] /= lu[k][k];
//             for (j = k + 1; j < size; j++)
//             {
//                 lu[i][j] -= (lu[i][k] * lu[k][j]);
//             }
//         }
//     }

//     // LUP solve
//     double *b = (double *)malloc(size * sizeof(double));
//     for (i = 0; i < size; i++)
//     {
//         b[i] = 0;
//     }
//     b[0] = 1;
//     double *y = (double *)malloc(size * sizeof(double));
//     for (k = 0; k < size; k++)
//     {
//         if (k > 0)
//         {
//             b[k - 1] = 0;
//             b[k] = 1;
//         }
//         for (i = 0; i < size; i++)
//         {
//             y[i] = b[p[i]];
//             for (j = 0; j < i; j++)
//             {
//                 y[i] -= (lu[i][j] * y[j]);
//             }
//         }
//         for (i = size - 1; i >= 0; i--)
//         {
//             arr[i][k] = y[i];
//             for (j = i + 1; j < size; j++)
//             {
//                 arr[i][k] -= (lu[i][j] * arr[j][k]);
//             }
//             arr[i][k] /= lu[i][i];
//         }
//     }
//     free(y);
//     free(b);
//     matrix_free(lu, size);
// }

// double **matrix_mul(double **result, double **x, double **y, int size)
// {
//     int i, j, k;
//     for (i = 0; i < size; i++)
//     {
//         for (j = 0; j < size; j++)
//         {
//             result[i][j] = 0;
//             for (k = 0; k < size; k++)
//             {
//                 result[i][j] += x[i][k] * y[k][j];
//             }
//         }
//     }
//     return result;
// }

int main()
{
#if defined ZSIM || defined SNIPER || defined PIMPROF
    PIMPROF_BEGIN_PROGRAM
#endif

    // srand(0);
    // double **arr = matrix_alloc(NUM1);
    // double **c = matrix_alloc(NUM1);
    // int i, j;
    // for (i = 0; i < NUM1; i++)
    // {
    //     for (j = 0; j < NUM1; j++)
    //     {
    //         arr[i][j] = rand() % 20 + 1;
    //     }
    // }
    // for (i = 0; i < NUM1; i++)
    // {
    //     for (j = 0; j < NUM1; j++)
    //     {
    //         c[i][j] = rand() % 20 + 1;
    //     }
    // }

    arr = (unsigned int *)aligned_alloc(LINE_SIZE, PAGE_SIZE * PAGE_MAX);
    INTNUM_IN_PAGE = PAGE_SIZE / sizeof(int);
    INTNUM_IN_LINE = LINE_SIZE / sizeof(int);
    int i, j, k, l, page_number, addr;

    printf("RATE=%d, RANDOM=%d, SEQUENTIAL=%d\n", RATE, RANDOM, SEQUENTIAL);
    printf("total random accesses=%d\n", RATE * RANDOM);
    printf("total sequential accesses=%d\n", RATE * RANDOM * (SEQUENTIAL - 1));
    printf("total multiplications=%d\n", RATE * RANDOM * SEQUENTIAL);
    unsigned rng = 1;

    int temp = 35;

#if defined ZSIM || defined SNIPER || defined PIMPROF
    PIMPROF_BEGIN_REG_PARALLEL
#endif

    // __asm__ __volatile__(
    //     "\tmov %1, %%rax \n"
    //     "\tmov %2, %%rbx \n"
    //     "\tadd %%rax, %%rbx \n"
    //     "\tmovq %%rbx, %0 \n"
    //     : "=m"(temp) /* output    */
    //     : "g"(temp),
    //       "g"(7) /* input */
    //     : "%rax", "%rbx"); /* clobbered */

    // #pragma omp parallel for collapse(2)
    for (i = 0; i < RATE; i++) {
        for (j = 0; j < RANDOM; j++) {
            rng = (rng * A + C) & M;
            page_number = rng % PAGE_MAX;
            for (k = 0; k < SEQUENTIAL; k++) {
                addr = page_number * INTNUM_IN_PAGE + k % INTNUM_IN_LINE;
                arr[addr] *= (arr[addr] + 1);
            }
        }
    }

    // for (i = 0; i < ITER1; i++)
    //     transpose(arr, NUM2);

#if defined ZSIM || defined SNIPER || defined PIMPROF
    PIMPROF_END_REG_PARALLEL
#endif
    printf("%d\n", temp);

    // pthreads_exec();
#if defined ZSIM || defined SNIPER || defined PIMPROF
    PIMPROF_END_PROGRAM
#endif
    return 0;
}
