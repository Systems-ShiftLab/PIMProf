CXX          := g++
CLANGXX      := clang++
CXXFLAGS     := -fPIC -std=c++11 #-pedantic-errors -Wall -Wextra -Werror
PASSFLAGS    := -Xclang -load -Xclang ../build/LLVMInjection/libLLVMInjection.so -fPIC -std=c++11
LDFLAGS      := -L/usr/lib -lstdc++ -lm -pthread #-lbacktrace -ldl
INCLUDE      :=

ANNOTATOR_DIR := ../LLVMInjection

ANNOTATOR_SRC := PIMProfAnnotator.cpp
INJECTED_SRC  := print4.cpp test.cpp

ANNOTATOR_OBJ := $(ANNOTATOR_SRC:%.cpp=%.o)
INJECTED_OBJ  := $(INJECTED_SRC:%.cpp=%.o)

ANNOTATOR_LL := $(ANNOTATOR_SRC:%.cpp=%.ll)
INJECTED_LL  := $(INJECTED_SRC:%.cpp=%.ll)

ANNOTATOR_BC := $(ANNOTATOR_SRC:%.cpp=%.bc)
INJECTED_BC  := $(INJECTED_SRC:%.cpp=%.bc)


all: $(ANNOTATOR_OBJ) $(INJECTED_OBJ)
	$(CLANGXX) $(CXXFLAGS) $(INCLUDE) -o test.exe $^ $(LDFLAGS)

$(ANNOTATOR_OBJ): %.o: $(ANNOTATOR_DIR)/%.cpp
	$(CLANGXX) $(CXXFLAGS) $(INCLUDE) -o $@ -c $<

$(INJECTED_OBJ): %.o: %.cpp
	$(CLANGXX) $(PASSFLAGS) $(INCLUDE) -o $@ -c $<


ll: $(ANNOTATOR_LL) $(INJECTED_LL)

$(ANNOTATOR_LL) : %.ll : $(ANNOTATOR_DIR)/%.cpp
	$(CLANGXX) $(PASSFLAGS) $(INCLUDE) -emit-llvm -o $@ -S $<

$(INJECTED_LL) : %.ll : %.cpp
	$(CLANGXX) $(PASSFLAGS) $(INCLUDE) -emit-llvm -o $@ -S $<


bc: $(ANNOTATOR_BC) $(INJECTED_BC)

$(ANNOTATOR_BC) : %.bc : $(ANNOTATOR_DIR)/%.cpp
	$(CLANGXX) $(PASSFLAGS) $(INCLUDE) -emit-llvm -o $@ -c $<

$(INJECTED_BC) : %.bc : %.cpp
	$(CLANGXX) $(PASSFLAGS) $(INCLUDE) -emit-llvm -o $@ -c $<


clean:
	rm -rf *.o *.exe *.bc *.ll
