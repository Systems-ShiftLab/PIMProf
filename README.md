# PIMProf
This project is compatible with LLVM 10 and clang 10

# Structure of repository
* `Configs/`: The configuration files of PIMProf. The default is `defaultconfig_32.ini`.
* `LLVMAnalysis/`: The tool for instrumenting the program. This is implemented as an LLVM pass and invoked by clang. This directory also contains some hooks that can be used for annotating region of interest.
* `PIMProfSolver/`: The Pin tool for analyzing the instrumented program.
* `test/`: The unit test.

# Prerequisite
Install llvm-10 and clang-10.
`
$ apt install clang-10 llvm-10` 

# Compilation

$ make -j

# Testing
PIMProf solver now depends on the runtime performance provided by simulators. We integrate our tool to Sniper in a separated repository:


# Detailed Usage
We use the unit test in `test/` as an example:
1. By default, we generate the LLVM pass as `build/LLVMAnalysis/libPIMProfAnnotation.so`. Invoke this LLVM pass by adding the following flags when compiling the source file with clang:
```
$ clang++-7 -Xclang -load -Xclang $(INJECTION_SO) ...
```
2. By default, we generate the Pin tool as `build/PIMProfSolver/PIMProfSolver.so`. Start PIMProf analysis by running the program with this Pin tool:
```
$ pin -t build/PIMProfSolver/PIMProfSolver.so -c Configs/defaultconfig_32.ini -o decision.out -- ./test.exe
```
Commonly used command line options:
```
-c <config filename>
        specify config file name
-o <decision output filename>
        specify file name containing PIM offloading decision
-roi
        specify whether ROI mode is enabled, if enabled, only regions between
        PIMProfROIHead and PIMProfROITail is analyzed
-roidecision
        specify whether ROI decision mode is enabled, if enabled, regions
        between PIMProfROIHead and PIMProfROITail will be offloaded to PIM and
        the rest stay on CPU
-s <statistics output filename>
        specify file name for statistics
```

The memory trace is output as: `test/MemTrace.out`.




# notes
https://stackoverflow.com/questions/8486314/setting-processor-affinity-with-c-that-will-run-on-linux

