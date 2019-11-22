include ../Makefile.config
include ../Makefile.common

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

INJCXXFLAGS := -Xclang -load -Xclang $(INJECTION_SO) -fPIC -std=c++11 -g
OFFCXXFLAGS := -Xclang -load -Xclang $(OFFLOADING_SO) -fPIC -std=c++11 -g
LDFLAGS  := -pthread

all: offload

offload: $(TEST_EXE_2)
	./$(TEST_EXE_2)


$(DECISION): $(TEST_EXE) $(TEST_SO)
	$(PIN_ROOT)/pin -t $(PININSTRUMENT_SO) -c $(DEFAULT_CONFIG) -o $@ -- ./$(TEST_EXE)

clean:
	rm -rf *.o *.exe *.bc *.ll *.out *.log *.dot

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

.PHONY: all offload clean
