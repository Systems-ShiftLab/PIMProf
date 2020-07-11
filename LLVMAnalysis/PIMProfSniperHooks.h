#ifndef __PIMPROF_SNIPERHOOKS__
#define __PIMPROF_SNIPERHOOKS__

#include "sim_api.h"

#if defined SNIPER && SNIPER == 0
    #define PIMPROF_BEGIN_PROGRAM SimRoiStart();
    #define PIMPROF_END_PROGRAM SimRoiEnd();
    #define PIMPROF_BEGIN_REG_PARALLEL SimRoiEnd();
    #define PIMPROF_END_REG_PARALLEL SimRoiStart();
    #warning SNIPER == 0
#elif defined SNIPER && SNIPER == 1
    #define PIMPROF_BEGIN_PROGRAM
    #define PIMPROF_END_PROGRAM
    #define PIMPROF_BEGIN_REG_PARALLEL SimRoiStart();
    #define PIMPROF_END_REG_PARALLEL SimRoiEnd();
    #warning SNIPER == 1
#elif defined SNIPER && SNIPER == 2
    #define PIMPROF_BEGIN_PROGRAM SimRoiStart();
    #define PIMPROF_END_PROGRAM SimRoiEnd();
    #define PIMPROF_BEGIN_REG_PARALLEL ;
    #define PIMPROF_END_REG_PARALLEL ;
    #warning SNIPER == 2
#elif defined SNIPER && SNIPER == 3
    #include "PIMProfAnnotation.h"
    #define PIMPROF_BEGIN_PROGRAM PIMProfROIDecisionBegin(); SimRoiStart();
    #define PIMPROF_END_PROGRAM PIMProfROIDecisionEnd(); SimRoiEnd();
    #define PIMPROF_BEGIN_REG_PARALLEL PIMProfROIBegin(); SimPimOffloadStart();
    #define PIMPROF_END_REG_PARALLEL PIMProfROIEnd(); SimPimOffloadEnd();
    #warning SNIPER == 3
#elif defined SNIPER && SNIPER == 4
    #include "PIMProfAnnotation.h"
    #define PIMPROF_BEGIN_PROGRAM PIMProfROIBegin(); SimRoiStart(); SimPimOffloadStart();
    #define PIMPROF_END_PROGRAM PIMProfROIEnd(); SimPimOffloadEnd(); SimRoiEnd();
    #define PIMPROF_BEGIN_REG_PARALLEL ;
    #define PIMPROF_END_REG_PARALLEL ;
    #warning SNIPER == 4
#elif defined SNIPER && SNIPER == 5
    #include "PIMProfAnnotation.h"
    #define PIMPROF_BEGIN_PROGRAM SimRoiStart(); SimPimOffloadStart();
    #define PIMPROF_END_PROGRAM SimPimOffloadEnd(); SimRoiEnd();
    #define PIMPROF_BEGIN_REG_PARALLEL SimPimOffloadEnd();
    #define PIMPROF_END_REG_PARALLEL SimPimOffloadStart();
    #warning SNIPER == 5
#elif defined SNIPER && SNIPER == 6
    #include "PIMProfAnnotation.h"
    #define PIMPROF_BEGIN_PROGRAM SimRoiStart();
    #define PIMPROF_END_PROGRAM SimRoiEnd();
    #define PIMPROF_BEGIN_REG_PARALLEL SimPimOffloadStart();
    #define PIMPROF_END_REG_PARALLEL SimPimOffloadEnd();
    #warning SNIPER == 6
#elif defined SNIPERTEST && SNIPERTEST == 0
    #define PIMPROF_BEGIN_PROGRAM SimRoiStart();
    #define PIMPROF_END_PROGRAM SimRoiEnd();
    #define PIMPROF_BEGIN_REG_PARALLEL SimRoiEnd();
    #define PIMPROF_END_REG_PARALLEL SimRoiStart();
    #warning SNIPERTEST == 0
#elif defined SNIPERTEST && SNIPERTEST == 1
    #define PIMPROF_BEGIN_PROGRAM
    #define PIMPROF_END_PROGRAM
    #define PIMPROF_BEGIN_REG_PARALLEL SimRoiStart();
    #define PIMPROF_END_REG_PARALLEL SimRoiEnd();
    #warning SNIPERTEST == 1
#elif defined SNIPERTEST && SNIPERTEST == 2
    #define PIMPROF_BEGIN_PROGRAM SimRoiStart(); SimPimOffloadStart();
    #define PIMPROF_END_PROGRAM SimRoiEnd(); SimPimOffloadEnd();
    #define PIMPROF_BEGIN_REG_PARALLEL SimPimOffloadEnd();
    #define PIMPROF_END_REG_PARALLEL SimPimOffloadStart();
    #warning SNIPERTEST == 2
#elif defined SNIPERTEST && SNIPERTEST == 3
    #define PIMPROF_BEGIN_PROGRAM SimRoiStart();
    #define PIMPROF_END_PROGRAM SimRoiEnd();
    #define PIMPROF_BEGIN_REG_PARALLEL SimPimOffloadStart();
    #define PIMPROF_END_REG_PARALLEL SimPimOffloadEnd();
    #warning SNIPERTEST == 3
#elif defined VTUNE
    #include <ittnotify.h>
    __itt_domain *vtune_domain = __itt_domain_create("pimprof.vtune_analysis");
    // vtune_domain->flags = 1;
    #define PIMPROF_BEGIN_PROGRAM __itt_resume();
    #define PIMPROF_END_PROGRAM __itt_detach();
    #define PIMPROF_BEGIN_REG_PARALLEL __itt_frame_begin_v3(vtune_domain, NULL);
    #define PIMPROF_END_REG_PARALLEL __itt_frame_end_v3(vtune_domain, NULL);
    #warning VTUNE
#elif
#endif

#endif // __PIMPROF_SNIPERHOOKS__
