include ../Makefile.config
include ../Makefile.common

PIMPROFOFFLOADER_SO := $(LLVM_BUILD_DIR)/libPIMProfOffloader.so

ifndef PIMPROF_DIR
$(error PIMPROF_DIR is not set in Makefile.config)
endif

DECISION            := decision.out
TEST_OBJ            := test.o print4.o pthreads.o
TEST_SO             := libtest.so
TEST_EXE            := test.exe
TEST_PY             := test.py

TEST_OBJ_2          := test.o.2 print4.o.2 pthreads.o.2
TEST_EXE_2          := test.exe.2

CXXFLAGS := -fPIC -std=c++11 -g -I$(PIMPROF_DIR)/LLVMAnalysis/
INJCXXFLAGS := -Xclang -load -Xclang $(INJECTION_SO) $(CXXFLAGS)
OFFCXXFLAGS := -Xclang -load -Xclang $(OFFLOADING_SO) $(CXXFLAGS)
LDFLAGS  := -pthread

all: offload

offload: $(TEST_EXE_2)
	./$(TEST_EXE_2)

$(DECISION): $(TEST_EXE) $(TEST_SO)
	$(PIN_ROOT)/pin -t $(PININSTRUMENT_SO) -c $(DEFAULT_CONFIG) -o $@ -- ./$(TEST_EXE)

$(TEST_EXE_2): $(TEST_OBJ_2) $(PIMPROFOFFLOADER_SO)
	$(CLANGXX) -o $@ $^ -L$(LLVM_BUILD_DIR) -Wl,-rpath=$(LLVM_BUILD_DIR) $(LDFLAGS)

$(TEST_EXE): $(TEST_OBJ) $(ANNOTATION_SO)
	$(CLANGXX) -o $@ $^ $(LDFLAGS)

$(TEST_SO): $(TEST_OBJ) $(ANNOTATION_SO)
	$(CLANGXX) -shared -fPIC -o $@ $^ $(LDFLAGS)

%.o.2: %.cpp $(DECISION)
	export PIMPROFDECISION=$(TEST_DIR)/$(DECISION) && $(CLANGXX) $(OFFCXXFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CLANGXX) $(INJCXXFLAGS) -S -emit-llvm $<
	$(CLANGXX) $(INJCXXFLAGS) -c -o $@ $<

test.zsim: test.cpp print4.cpp pthreads.cpp
	$(CLANGXX) $(CXXFLAGS) -DZSIM=3 -fopenmp -o $@ $^ -L$(LLVM_BUILD_DIR) -Wl,-rpath=$(LLVM_BUILD_DIR) -lzsimhooks $(LDFLAGS)

clean:
	rm -rf *.o *.exe *.bc *.ll *.out *.log *.dot test.zsim

.PHONY: all offload clean
