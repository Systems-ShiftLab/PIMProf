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

CXXFLAGS := -Xclang -load -Xclang $(INJECTION_SO) -fPIC -std=c++11 -g
LDFLAGS  := -pthread

all: $(DECISION)

$(DECISION): $(TEST_EXE) $(TEST_SO)
	$(PIN_ROOT)/pin -t $(PININSTRUMENT_SO) -c $(DEFAULT_CONFIG) -o $@ -- ./$(TEST_EXE)

clean:
	rm -rf *.o *.exe *.bc *.ll *.out *.log *.dot

$(TEST_EXE): $(TEST_OBJ) $(ANNOTATION_SO)
	$(CLANGXX) -o $@ $^ $(LDFLAGS)

$(TEST_SO): $(TEST_OBJ) $(ANNOTATION_SO)
	$(CLANGXX) -shared -fPIC -o $@ $^ $(LDFLAGS)


%.o: %.cpp
	$(CLANGXX) $(CXXFLAGS) -S -emit-llvm $<
	$(CLANGXX) $(CXXFLAGS) -c -o $@ $<

.PHONY: all clean
