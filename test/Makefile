include ../Makefile.config
include ../Makefile.common

PIMPROFOFFLOADER_SO := $(LLVM_BUILD_DIR)/libPIMProfOffloader.so

ifndef PIMPROF_DIR
$(error PIMPROF_DIR is not set in Makefile.config)
endif

DECISION            := decision.out
TEST_OBJ            := test.o # print4.o pthreads.o
TEST_SO             := libtest.so
TEST_EXE            := test.exe
TEST_PY             := test.py

TEST_OBJ_2          := test.o.2 print4.o.2 pthreads.o.2
TEST_EXE_2          := test.exe.2

CXXFLAGS := -fPIC -std=c++11 -g -I$(PIMPROF_DIR)/LLVMAnalysis/ -fopenmp -O3 -mno-sse4 -mno-sse4.1 -mno-sse4.2 -mno-sse4a -mno-avx -mno-avx2 -DPIMPROF_DEBUG
INJCXXFLAGS := $(CXXFLAGS) -Xclang -load -Xclang $(INJECTION_SO)
OFFCXXFLAGS := $(CXXFLAGS) -Xclang -load -Xclang $(OFFLOADING_SO)
LDFLAGS  := -pthread

all: offload

offload: $(TEST_EXE_2)
	./$(TEST_EXE_2)

$(DECISION): $(TEST_EXE) $(TEST_SO)
	export OMP_NUM_THREADS=1 && $(PIN_ROOT)/pin -t $(PININSTRUMENT_SO) -c $(DEFAULT_CONFIG) -o $@ -- ./$(TEST_EXE)

$(TEST_EXE_2): $(TEST_OBJ_2) $(PIMPROFOFFLOADER_SO)
	$(CLANGXX) $(OFFCXXFLAGS) -o $@ $^ -L$(LLVM_BUILD_DIR) -Wl,-rpath=$(LLVM_BUILD_DIR) $(LDFLAGS)

$(TEST_EXE): $(TEST_OBJ) $(ANNOTATION_SO)
	$(CLANGXX) $(INJCXXFLAGS) -o $@ $^ $(LDFLAGS)

$(TEST_SO): $(TEST_OBJ) $(ANNOTATION_SO)
	$(CLANGXX) -shared -fPIC -o $@ $^ $(LDFLAGS)

%.o.2: %.cpp $(DECISION)
	export PIMPROFDECISION=$(TEST_DIR)/$(DECISION) && export PIMPROFROI=NOTPIM && $(CLANGXX) $(OFFCXXFLAGS) -DPIMPROF -c -o $@ $< > test.dump 2>&1

%.o: %.cpp
	$(CLANGXX) $(INJCXXFLAGS) -DPIMPROF -S -emit-llvm $<
	$(CLANGXX) $(INJCXXFLAGS) -DPIMPROF -c -o $@ $<

test.zsim: test.cpp print4.cpp pthreads.cpp
	$(CLANGXX) $(CXXFLAGS) -DZSIM=1 -o $@ $^

test.sniper: test.cpp print4.cpp pthreads.cpp
	$(CLANGXX) $(SNIPER_CXX_FLAGS) $(SNIPER_CFLAGS) -DSNIPER=1 -o $@ $^ -lm -static -L/home/warsier/Documents/sniper_pim_prof/sniper/lib

clean:
	rm -rf *.o *.exe *.bc *.ll *.out *.log *.dot test.zsim test.dump

.PHONY: all offload clean
