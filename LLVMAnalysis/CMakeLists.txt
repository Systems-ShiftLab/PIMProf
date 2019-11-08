set(ANNOTATIONINJECTION "AnnotationInjection")
set(ANNOTATIONINJECTION_SRC "${ANNOTATIONINJECTION}.cpp")
add_library(${ANNOTATIONINJECTION} MODULE
    ${ANNOTATIONINJECTION_SRC} "MurmurHash3.cpp")
target_compile_features(${ANNOTATIONINJECTION} PRIVATE
    cxx_range_for cxx_auto_type)
set_target_properties(${ANNOTATIONINJECTION} PROPERTIES
    COMPILE_FLAGS "-fno-rtti -fPIC")

set(OFFLOADERINJECTION "OffloaderInjection")
set(OFFLOADERINJECTION_SRC "${OFFLOADERINJECTION}.cpp")
add_library(${OFFLOADERINJECTION} MODULE
    ${OFFLOADERINJECTION_SRC} "MurmurHash3.cpp")
target_compile_features(${OFFLOADERINJECTION} PRIVATE
    cxx_range_for cxx_auto_type)
set_target_properties(${OFFLOADERINJECTION} PROPERTIES
    COMPILE_FLAGS "-fno-rtti -fPIC")

set(ANNOTATIONGENERATION "AnnotationGeneration")
set(ANNOTATIONGENERATION_SRC "${ANNOTATIONGENERATION}.cpp")
set(ANNOTATIONGENERATION_EXE "${ANNOTATIONGENERATION}.exe")
add_executable(${ANNOTATIONGENERATION_EXE}
    ${ANNOTATIONGENERATION_SRC})
target_link_libraries(${ANNOTATIONGENERATION_EXE} LLVM)
target_compile_features(${ANNOTATIONGENERATION_EXE} PRIVATE
    cxx_range_for cxx_auto_type)
set_target_properties(${ANNOTATIONGENERATION_EXE} PROPERTIES
    COMPILE_FLAGS "-fno-rtti -fPIC")

# set(CFGDUMP "CFGDump")
# set(CFGDUMP_SRC "${CFGDUMP}.cpp")
# set(CFGDUMP_EXE "${CFGDUMP}.exe")
# add_executable(${CFGDUMP_EXE}
#    ${CFGDUMP_SRC})
# target_link_libraries(${CFGDUMP_EXE} LLVM)
# target_compile_features(${CFGDUMP_EXE} PRIVATE
#     cxx_range_for cxx_auto_type)
# set_target_properties(${CFGDUMP_EXE} PROPERTIES
#     COMPILE_FLAGS "-fno-rtti")
