# set the targets
set(LLVMPASS_SRC "AnnotationInjection.cpp" "OffloaderInjection.cpp")
set(LLVMEXE_SRC "AnnotationGeneration.cpp")
set(LIB_SRC "PIMProfOffloader.cpp" "PIMProfZSimHooks.cpp" "PIMProfTimer.cpp")

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

# process LLVMEXE targets
foreach(SRCFILE ${LLVMEXE_SRC})
    get_filename_component(SRCNAME ${SRCFILE} NAME_WE)
    set(EXE ${SRCNAME}.exe)
    add_executable(${EXE}
        ${SRCFILE})
    target_link_libraries(${EXE} LLVM)
    target_compile_features(${EXE} PRIVATE
        cxx_range_for cxx_auto_type)
    set_target_properties(${EXE} PROPERTIES
        COMPILE_FLAGS "-fno-rtti -fPIC")
endforeach(SRCFILE ${LLVMEXE_SRC})

# process other LIB targets
foreach(SRCFILE ${LIB_SRC})
    get_filename_component(SRCNAME ${SRCFILE} NAME_WE)
    add_library(${SRCNAME} SHARED
        ${SRCFILE})
    target_compile_features(${SRCNAME} PRIVATE
        cxx_range_for cxx_auto_type)
    set_target_properties(${SRCNAME} PROPERTIES
        COMPILE_FLAGS "-fno-rtti")
    include_directories("~/intel/vtune_profiler/include")
endforeach(SRCFILE ${LIB_SRC})


