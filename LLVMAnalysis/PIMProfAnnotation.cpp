#include "PIMProfAnnotation.h"
#include "ittnotify.h"
#include "Common.h"
#include <string>
#include <iostream>

int PIMProf_prev_mode = -1;
uint64_t PIMProf_prev_uuid_hi = -1;
uint64_t PIMProf_prev_uuid_lo = -1;
PIMProf::UUIDHashMap<__itt_domain *> PIMProfDomain;

int PIMProfVTuneAnnotation(int mode, uint64_t uuid_hi, uint64_t uuid_lo) {
    if (PIMProf_prev_mode == mode && PIMProf_prev_uuid_hi == uuid_hi && PIMProf_prev_uuid_lo == uuid_lo) {
        std::cout << "REPEAT " << mode << " " << (int64_t)uuid_hi << " " << (int64_t)uuid_lo << std::endl;
        return 0;
    }
    if (mode == PIMProf::VTUNE_MODE_FRAME_BEGIN) {
        PIMProf_prev_mode = mode;
        PIMProf_prev_uuid_hi = uuid_hi;
        PIMProf_prev_uuid_lo = uuid_lo;
        std::cout << "FRAME_BEGIN " << (int64_t)uuid_hi << " " << (int64_t)uuid_lo << std::endl;
        PIMProf::UUID bblhash(uuid_hi, uuid_lo);
        auto p = PIMProfDomain.find(bblhash);
        if (p == PIMProfDomain.end()) {
            std::string name = "PIMProf." + std::to_string(uuid_hi) + "." + std::to_string(uuid_lo);
            std::cout << name << std::endl;
            __itt_domain *temp = __itt_domain_create(name.c_str());
            p = PIMProfDomain.insert(std::make_pair(bblhash, temp)).first;
            p->second->flags = 1;
        }
        __itt_frame_begin_v3(p->second, NULL);
        
        return 0;
    }
    if (mode == PIMProf::VTUNE_MODE_FRAME_END) {
        PIMProf_prev_mode = mode;
        PIMProf_prev_uuid_hi = uuid_hi;
        PIMProf_prev_uuid_lo = uuid_lo;
        std::cout << "FRAME_END " << uuid_hi << " " << uuid_lo << std::endl;
        PIMProf::UUID bblhash(uuid_hi, uuid_lo);
        auto p = PIMProfDomain.find(bblhash);
        if (p == PIMProfDomain.end()) {
            __itt_domain *temp = __itt_domain_create((
                "PIMProf." + std::to_string(uuid_hi) + "." + std::to_string(uuid_lo)).c_str());
            p = PIMProfDomain.insert(std::make_pair(bblhash, temp)).first;
        }
        __itt_frame_end_v3(p->second, NULL);
        
        return 0;
    }
    if (mode == PIMProf::VTUNE_MODE_RESUME) {
        PIMProf_prev_mode = mode;
        PIMProf_prev_uuid_hi = uuid_hi;
        PIMProf_prev_uuid_lo = uuid_lo;
        std::cout << "RESUME" << std::endl;
        __itt_resume();
        return 0;
    }
    if (mode == PIMProf::VTUNE_MODE_PAUSE) {
        PIMProf_prev_mode = mode;
        PIMProf_prev_uuid_hi = uuid_hi;
        PIMProf_prev_uuid_lo = uuid_lo;
        std::cout << "PAUSE" << std::endl;
        __itt_pause();
        return 0;
    }
    if (mode == PIMProf::VTUNE_MODE_DETACH) {
        PIMProf_prev_mode = mode;
        PIMProf_prev_uuid_hi = uuid_hi;
        PIMProf_prev_uuid_lo = uuid_lo;
        std::cout << "DETACH" << std::endl;
        __itt_detach();
        return 0;
    }
    return 0;
}