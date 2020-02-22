#ifndef __PIMPROF_SNIPERHOOKS__
#define __PIMPROF_SNIPERHOOKS__

#include "sim_api.h"

#if SNIPER == 0
    #define PIMPROF_BEGIN_PROGRAM SimRoiStart();
    #define PIMPROF_END_PROGRAM SimRoiEnd();
    #define PIMPROF_BEGIN_REG_PARALLEL SimRoiEnd();
    #define PIMPROF_END_REG_PARALLEL SimRoiStart();
    #warning SNIPER == 0
#elif SNIPER == 1
    #define PIMPROF_BEGIN_PROGRAM SimRoiStart();
    #define PIMPROF_END_PROGRAM SimRoiEnd();
    #define PIMPROF_BEGIN_REG_PARALLEL SimPimOffloadStart();
    #define PIMPROF_END_REG_PARALLEL SimPimOffloadEnd();
    #warning SNIPER == 1
#elif SNIPER == 2
    #define PIMPROF_BEGIN_PROGRAM SimRoiStart();
    #define PIMPROF_END_PROGRAM SimRoiEnd();
    #define PIMPROF_BEGIN_REG_PARALLEL ;
    #define PIMPROF_END_REG_PARALLEL ;
    #warning SNIPER == 2
#elif SNIPER == 3 
    #include "PIMProfAnnotation.h"
    #define PIMPROF_BEGIN_PROGRAM PIMProfROIDecisionBegin(); SimRoiStart();
    #define PIMPROF_END_PROGRAM PIMProfROIDecisionEnd(); SimRoiEnd();
    #define PIMPROF_BEGIN_REG_PARALLEL PIMProfROIBegin(); SimPimOffloadStart();
    #define PIMPROF_END_REG_PARALLEL PIMProfROIEnd(); SimPimOffloadEnd();
    #warning SNIPER == 3
#elif SNIPER == 4 
    #include "PIMProfAnnotation.h"
    #define PIMPROF_BEGIN_PROGRAM PIMProfROIBegin(); SimRoiStart(); SimPimOffloadStart();
    #define PIMPROF_END_PROGRAM PIMProfROIEnd(); SimPimOffloadEnd(); SimRoiEnd();
    #define PIMPROF_BEGIN_REG_PARALLEL ;
    #define PIMPROF_END_REG_PARALLEL ;
    #warning SNIPER == 4
#else
#endif

#endif // __PIMPROF_SNIPERHOOKS__
