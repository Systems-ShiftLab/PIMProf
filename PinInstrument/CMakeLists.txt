set(SRCFILE "main.cpp" ) #"CostPackage.cpp" "CostSolver.cpp"

set(EXE Solver.exe)
add_executable(${EXE}
    ${SRCFILE})
set_target_properties(${EXE} PROPERTIES
        COMPILE_FLAGS "-fno-rtti -fPIC")