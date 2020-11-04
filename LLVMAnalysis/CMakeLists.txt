# set the targets
set(LLVMPASS_SRC "AnnotationInjection.cpp" "OffloaderInjection.cpp")

# process LLVMPASS targets
foreach(SRCFILE ${LLVMPASS_SRC})
    get_filename_component(SRCNAME ${SRCFILE} NAME_WE)
    add_library(${SRCNAME} SHARED
        ${SRCFILE} "MurmurHash3.cpp")
    target_compile_features(${SRCNAME} PRIVATE
        cxx_range_for cxx_auto_type)
    set_target_properties(${SRCNAME} PROPERTIES
        COMPILE_FLAGS "-fno-rtti -fPIC")
endforeach(SRCFILE ${LLVMPASS_SRC})



