clang++ -Xclang -load -Xclang ../build/LLVMInjection/libLLVMInjection.so ../LLVMInjection/PIMProfAnnotator.cpp test.cpp -o test.exe
# clang++ -Xclang -load -Xclang ../build/LLVMInjection/libLLVMInjection.so test.cpp -o test.exe
