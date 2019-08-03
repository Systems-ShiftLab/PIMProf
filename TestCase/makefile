BUILD_DIR           := ../build
ANALYSIS_BUILD_DIR  := $(BUILD_DIR)/LLVMAnalysis

LLVM_BIN         := $(LLVM_HOME)/bin
CLANGXX          := $(LLVM_BIN)/clang++
OPT              := $(LLVM_BIN)/opt
LLC              := $(LLVM_BIN)/llc
LINK             := $(LLVM_BIN)/llvm-link
DIS              := $(LLVM_BIN)/llvm-dis
CXXFLAGS := -fPIC -std=c++11 -O3
EMITBC_CXXFLAGS  := -fPIC -std=c++11 -flto # -O3 -pedantic-errors -Wall -Wextra -Werror

LDFLAGS          := 
INCLUDE          :=


ANNOTATOR_BC  := PIMProfAnnotator.bc

OFFLOADER_SRC := PIMProfOffloader.cpp
OFFLOADER_BC  := $(OFFLOADER_SRC:%.cpp=%.bc)

EXTERNAL_SRC  := print4.cpp test.cpp
EXTERNAL_BC   := $(EXTERNAL_SRC:%.cpp=%.bc)
EXTERNAL_MERGED_BC := external_merged.bc
EXTERNAL_OPT_BC    := $(EXTERNAL_MERGED_BC:%.bc=%.opt.bc)

MERGED_BC     := merged.bc
MERGED_LL     := $(MERGED_BC:%.bc=%.ll)
MERGED_OPT_BC := $(MERGED_BC:%.bc=%.opt.bc)
MERGED_DUMP   := $(MERGED_BC:%.bc=%.out)

TEST_EXE      := test.exe

DECISION      := decision.out
DECISION_BC   := decision.bc

FINAL_BC      := final.bc
FINAL_LL      := $(FINAL_BC:%.bc=%.ll)
FINAL_EXE     := $(FINAL_BC:%.bc=%.exe)

# Process TestCase folder
all: cmake

# generate LLVM analysis tools
cmake:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake ..
	make -C $(BUILD_DIR)

pin: cmake $(FINAL_EXE)

$(DECISION): $(TEST_EXE) $(MERGED_DUMP) all
	make -C ../PinInstrument/
	$(PIN_ROOT)/pin -t ../PinInstrument/obj-intel64/PinInstrument.so  -b $(MERGED_DUMP) -o $@ -- ./$(TEST_EXE)
	
# generate the bitcode of each source code
$(EXTERNAL_BC): %.bc: %.cpp
	$(CLANGXX) $(EMITBC_CXXFLAGS) $(INCLUDE) -o $@ -c $<

# generate the annotator function itself
$(ANNOTATOR_BC): cmake
	$(ANALYSIS_BUILD_DIR)/AnnotatorGeneration.exe

# link bitcodes
$(EXTERNAL_MERGED_BC): $(EXTERNAL_BC)
	$(LINK) -o $@ $^

# inject annotator
$(EXTERNAL_OPT_BC): $(EXTERNAL_MERGED_BC)
	$(OPT) -load $(ANALYSIS_BUILD_DIR)/libAnnotatorInjection.so -AnnotatorInjection -o $@ $<

# link the source file with annotator function file
# note that the annotator bc itself should not be annotated
$(MERGED_BC): $(EXTERNAL_OPT_BC) $(ANNOTATOR_BC)
	$(LINK) -o $@ $^
	$(DIS) $@ -o $(MERGED_LL)

$(OFFLOADER_BC): ../LLVMAnalysis/$(OFFLOADER_SRC) cmake
	$(CLANGXX) $(EMITBC_CXXFLAGS) $(INCLUDE) -o $@ -c $< $(LDFLAGS)

$(MERGED_DUMP): $(EXTERNAL_OPT_BC)
	$(ANALYSIS_BUILD_DIR)/CFGDump.exe -o $@ $^

$(TEST_EXE): $(MERGED_BC)
	$(CLANGXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^ $(LDFLAGS)
	# objdump -d $(TEST_EXE) > objdump$$LEVEL.out

$(DECISION_BC): $(EXTERNAL_MERGED_BC) $(DECISION)
	$(OPT) -load $(ANALYSIS_BUILD_DIR)/libOffloaderInjection.so -OffloaderInjection -decision=$(DECISION) $(EXTERNAL_MERGED_BC) -o $@

$(FINAL_BC): $(DECISION_BC) $(OFFLOADER_BC)
	$(LINK) -o $@ $^
	$(DIS) $@ -o $(FINAL_LL)

$(FINAL_EXE): $(FINAL_BC)
	$(CLANGXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^ $(LDFLAGS)


clean:
	rm -rf *.o *.exe *.bc *.ll *.out *.log *.dot
	rm -rf $(BUILD_DIR)
	make clean -C ../PinInstrument/

.PHONY: all bitcode cmake offloader pin clean
