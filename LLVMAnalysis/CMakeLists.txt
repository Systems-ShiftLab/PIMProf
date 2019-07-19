set(ANNOTATORINJECTION "AnnotatorInjection")
set(ANNOTATORINJECTION_SRC "${ANNOTATORINJECTION}.cpp")
add_library(${ANNOTATORINJECTION} MODULE
    ${ANNOTATORINJECTION_SRC})
target_compile_features(${ANNOTATORINJECTION} PRIVATE
    cxx_range_for cxx_auto_type)
set_target_properties(${ANNOTATORINJECTION} PROPERTIES
    COMPILE_FLAGS "-fno-rtti")

set(OFFLOADERINJECTION "OffloaderInjection")
set(OFFLOADERINJECTION_SRC "${OFFLOADERINJECTION}.cpp")
add_library(${OFFLOADERINJECTION} MODULE
    ${OFFLOADERINJECTION_SRC})
target_compile_features(${OFFLOADERINJECTION} PRIVATE
    cxx_range_for cxx_auto_type)
set_target_properties(${OFFLOADERINJECTION} PROPERTIES
    COMPILE_FLAGS "-fno-rtti")

set(ANNOTATORGENERATION "AnnotatorGeneration")
set(ANNOTATORGENERATION_SRC "${ANNOTATORGENERATION}.cpp")
set(ANNOTATORGENERATION_EXE "${ANNOTATORGENERATION}.exe")
add_executable(${ANNOTATORGENERATION_EXE}
    ${ANNOTATORGENERATION_SRC})
target_link_libraries(${ANNOTATORGENERATION_EXE} LLVM)
target_compile_features(${ANNOTATORGENERATION_EXE} PRIVATE
    cxx_range_for cxx_auto_type)
set_target_properties(${ANNOTATORGENERATION_EXE} PROPERTIES
    COMPILE_FLAGS "-fno-rtti")

set(CFGDUMP "CFGDump")
set(CFGDUMP_SRC "${CFGDUMP}.cpp")
set(CFGDUMP_EXE "${CFGDUMP}.exe")
add_executable(${CFGDUMP_EXE}
   ${CFGDUMP_SRC})
target_link_libraries(${CFGDUMP_EXE} LLVM)
target_compile_features(${CFGDUMP_EXE} PRIVATE
    cxx_range_for cxx_auto_type)
set_target_properties(${CFGDUMP_EXE} PROPERTIES
    COMPILE_FLAGS "-fno-rtti")