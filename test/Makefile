PIMPROF_DIR         := $(shell pwd)/../
include ../Makefile.config
include ../Makefile.common

DECISION            := decision.out
TEST_EXE            := test.exe

CXXFLAGS := -fPIC -std=c++11 -O3
LDFLAGS  :=

all: $(DECISION)

$(DECISION): $(TEST_EXE)
	$(PIN_ROOT)/pin -t $(PININSTRUMENT_SO) -c $(DEFAULT_CONFIG) -o $@ -- ./$(TEST_EXE)

clean:
	rm -rf *.o *.exe *.bc *.ll *.out *.log *.dot

$(TEST_EXE): test.o print4.o $(ANNOTATION_SO)
	$(CLANGXX) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CLANGXX) -Xclang -load -Xclang $(INJECTION_SO) $(CXXFLAGS) -c -o $@ $<

.PHONY: all clean