# process LLVMPASS targets
set(ANNOTATION_INJECTION AnnotationInjection)
set(OFFLOADER_INJECTION OffloaderInjection)

foreach(TARGET ${ANNOTATION_INJECTION} ${OFFLOADER_INJECTION})
    set(SRCFILE ${TARGET}.cpp)
    add_library(${TARGET} SHARED
        ${SRCFILE} MurmurHash3.cpp)
    target_compile_features(${TARGET} PRIVATE
        cxx_range_for cxx_auto_type)
    set_target_properties(${TARGET} PROPERTIES
        COMPILE_FLAGS "-fno-rtti -fPIC")
endforeach()

# process PIMProf static library targets
set(PIMPROF_ANNOTATION PIMProfAnnotation)

add_library(${PIMPROF_ANNOTATION} STATIC
    ${PIMPROF_ANNOTATION}.cpp)
target_include_directories(${PIMPROF_ANNOTATION} PRIVATE
    ${VTUNE_HOME}/include/)
target_link_directories(${PIMPROF_ANNOTATION} PRIVATE
    ${VTUNE_HOME}/lib64/)
target_link_libraries(${PIMPROF_ANNOTATION}
    libittnotify.a)
