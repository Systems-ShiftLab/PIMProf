CXX          := g++
CLANGXX      := clang++
CXXFLAGS     := -fPIC -std=c++11 #-pedantic-errors -Wall -Wextra -Werror
CLANGXXFLAGS := -Xclang -load -Xclang ../build/LLVMInjection/libLLVMInjection.so -fPIC -std=c++11 
LDFLAGS      := -L/usr/lib -lstdc++ -lm -pthread #-lbacktrace -ldl
INCLUDE      :=

ANNOTATOR_DIR := ../LLVMInjection

ANNOTATOR_SRC := PIMProfAnnotator.cpp
INJECTED_SRC  := print4.cpp test.cpp

ANNOTATOR_OBJ := $(ANNOTATOR_SRC:%.cpp=%.o)
INJECTED_OBJ  := $(INJECTED_SRC:%.cpp=%.o)


all: $(ANNOTATOR_OBJ) $(INJECTED_OBJ)
	echo $(ANNOTATOR_OBJ)
	echo $(INJECTED_OBJ)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o  $^ $(LDFLAGS)

$(ANNOTATOR_OBJ): %o: $(ANNOTATOR_DIR)/%cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ -c $<

$(INJECTED_OBJ): %o: %cpp
	$(CLANGXX) $(CLANGXXFLAGS) $(INCLUDE) -o $@ -c $<

clean:
	rm -rf *.o