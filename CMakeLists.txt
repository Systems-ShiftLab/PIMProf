cmake_minimum_required(VERSION 3.4)
project(PIMProf)

set(ENV{LLVM_HOME} "/usr/lib/llvm-7")

if(NOT DEFINED ENV{LLVM_HOME})
    message(FATAL_ERROR "$LLVM_HOME is not defined")
else ()
    set(ENV{LLVM_DIR} "$ENV{LLVM_HOME}/lib/cmake/llvm")
endif()

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS} "PIMProfSolver")
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory("LLVMAnalysis")
add_subdirectory("PIMProfSolver")
