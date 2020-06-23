include Makefile.config
include Makefile.common

ifndef PIMPROF_DIR
$(error PIMPROF_DIR is not set in Makefile.config)
endif

all: llvm pin lib

llvm: build
	cd $(BUILD_DIR) && LLVM_HOME=$(LLVM_HOME) cmake ..
	make -C $(BUILD_DIR) VERBOSE=1

pin: build
	mkdir -p $(PIN_BUILD_DIR)
	make -C $(PIN_SRC_DIR) PIN_ROOT=$(PIN_ROOT) OBJDIR=$(PIN_BUILD_DIR) PIMPROF_MPKI=1

lib: $(ANNOTATION_SO)

$(ANNOTATION_SO): $(ANNOTATION_BC)
	$(CLANGXX) -shared -o $@ $<

$(ANNOTATION_BC): llvm
	$(LLVM_BUILD_DIR)/AnnotationGeneration.exe -o $(ANNOTATION_BC)

test: all
	make -C $(TEST_DIR)

testzsim: all
	make test.zsim -C $(TEST_DIR)

build:
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
	make clean -C $(TEST_DIR)

.PHONY: all llvm lib pin test build clean

