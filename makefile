include makefile.vars
LLVM_BIN         := $(LLVM_HOME)/bin
CLANGXX          := $(LLVM_BIN)/clang++

PIMPROF_DIR         := $(shell pwd)

LLVM_SRC_DIR        := LLVMAnalysis
PIN_SRC_DIR         := PinInstrument

BUILD_DIR           := $(PIMPROF_DIR)/build/
LLVM_BUILD_DIR      := $(BUILD_DIR)/LLVMAnalysis/
PIN_BUILD_DIR       := $(BUILD_DIR)/PinInstrument/

ANNOTATOR_BC        := $(LLVM_BUILD_DIR)/libPIMProfAnnotator.bc
ANNOTATOR_SO        := $(ANNOTATOR_BC:%.bc=%.so)


all: llvm pin lib

llvm: build
	cd $(BUILD_DIR) && LLVM_HOME=$(LLVM_HOME) cmake ..
	make -C $(BUILD_DIR)

pin: build
	mkdir -p $(PIN_BUILD_DIR)
	cd $(PIN_SRC_DIR) && make PIN_ROOT=$(PIN_ROOT) OBJDIR=$(PIN_BUILD_DIR)

lib: $(ANNOTATOR_SO)

$(ANNOTATOR_SO): $(ANNOTATOR_BC)
	$(CLANGXX) -shared -o $@ $<

$(ANNOTATOR_BC): llvm
	$(LLVM_BUILD_DIR)/AnnotatorGeneration.exe -o $(ANNOTATOR_BC)

test: all

build:
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)


.PHONY: all llvm lib pin test build clean

