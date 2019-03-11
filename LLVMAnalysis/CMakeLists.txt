set(ANNOTATORINJECTION "AnnotatorInjection")
add_library(${ANNOTATORINJECTION} MODULE 
    AnnotatorInjection.cpp)
target_compile_features(${ANNOTATORINJECTION} PRIVATE
    cxx_range_for cxx_auto_type)
set_target_properties(${ANNOTATORINJECTION} PROPERTIES
    COMPILE_FLAGS "-fno-rtti")

set(ANNOTATORGENERATION "AnnotatorGeneration.exe")
add_executable(${ANNOTATORGENERATION}
    AnnotatorGeneration.cpp)
target_link_libraries(${ANNOTATORGENERATION} LLVM)
target_compile_features(${ANNOTATORGENERATION} PRIVATE
    cxx_range_for cxx_auto_type)
set_target_properties(${ANNOTATORGENERATION} PROPERTIES
    COMPILE_FLAGS "-fno-rtti")

set(CFGDUMP "CFGDump")
add_library(${CFGDUMP} MODULE 
    CFGDump.cpp)
target_compile_features(${CFGDUMP} PRIVATE
    cxx_range_for cxx_auto_type)
set_target_properties(${CFGDUMP} PROPERTIES
    COMPILE_FLAGS "-fno-rtti")