include makefile.vars

LLVM_SRC_DIR        := LLVMAnalysis
PIN_SRC_DIR         := PinInstrument

BUILD_DIR           := build
LLVM_BUILD_DIR      := $(BUILD_DIR)/LLVMAnalysis
PIN_BUILD_DIR       := $(BUILD_DIR)/PinInstrument

CXXFLAGS := -fPIC -std=c++11 -O3
EMITBC_CXXFLAGS  := -fPIC -std=c++11 -flto # -O3 -pedantic-errors -Wall -Wextra -Werror

LDFLAGS          := 
INCLUDE          :=

all: llvm pin

llvm: build
	cd $(BUILD_DIR) && LLVM_HOME=$(LLVM_HOME) cmake ..
	make -C $(BUILD_DIR)

pin: build
	mkdir -p $(PIN_BUILD_DIR)
	cd $(PIN_SRC_DIR) && make PIN_ROOT=$(PIN_ROOT) OBJDIR=$(PIN_BUILD_DIR)

build:
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)


.PHONY: all llvm pin build

