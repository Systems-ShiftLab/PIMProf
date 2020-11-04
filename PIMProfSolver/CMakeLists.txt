set(SRCFILE
    "main.cpp"
    "Util.cpp"
    "CostSolver.cpp"
)

set(EXE Solver.exe)
add_executable(${EXE}
    ${SRCFILE}
)
target_include_directories(${EXE} PRIVATE ".")
set_target_properties(${EXE} PROPERTIES
        COMPILE_FLAGS "-fno-rtti -fPIC")

set(CMAKE_CXX_FLAGS "-g")