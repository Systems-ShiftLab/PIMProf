BUILD_DIR           := ../build
ANALYSIS_BUILD_DIR  := $(BUILD_DIR)/LLVMAnalysis

CXX              := clang++-7
OPT              := opt-7
LLC              := llc-7
LINK             := llvm-link-7
DIS              := llvm-dis-7
PREOPT_CXXFLAGS  := -flto -fPIC -std=c++11 # -O3 -pedantic-errors -Wall -Wextra -Werror
POSTOPT_CXXFLAGS := $(PREOPT_CXXFLAGS) -O3
PASSFLAGS        := -Xclang -load -Xclang $(ANALYSIS_BUILD_DIR)/libAnnotatorInjection.so
LDFLAGS          := 
INCLUDE          :=


ANNOTATOR_BC  := PIMProfAnnotator.bc

OFFLOADER_SRC := PIMProfOffloader.cpp
OFFLOADER_BC  := $(OFFLOADER_SRC:%.cpp=%.bc)

EXTERNAL_SRC  := print4.cpp test.cpp
EXTERNAL_OBJ  := $(EXTERNAL_SRC:%.cpp=%.o)
EXTERNAL_BC   := $(EXTERNAL_SRC:%.cpp=%.bc)

MERGED_BC     := merged.bc
MERGED_LL     := $(MERGED_BC:%.bc=%.ll)
MERGED_DUMP   := $(MERGED_BC:%.bc=%.out)

TEST_EXE      := test.exe

DECISION      := decision.out

# Process TestCase folder
all: cmake bitcode 

# link the source file with annotator function file, then compile to executable
bitcode: $(ANNOTATOR_BC) $(EXTERNAL_OBJ)
	$(LINK) -o $(MERGED_BC) $(ANNOTATOR_BC) $(EXTERNAL_OBJ)
	$(DIS) $(MERGED_BC) -o $(MERGED_LL)

# generate LLVM analysis tools
cmake:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake ..
	make -C $(BUILD_DIR)

pin: $(OFFLOADER_BC) all
	make -C ../PinInstrument/
	$(CXX) $(POSTOPT_CXXFLAGS) $(INCLUDE) -o $(TEST_EXE) $(MERGED_BC) $(LDFLAGS)
	$(ANALYSIS_BUILD_DIR)/CFGDump.exe $(MERGED_BC) $(MERGED_DUMP)
	objdump -d $(TEST_EXE) > objdump$$LEVEL.out
	$(PIN_ROOT)/pin -t ../PinInstrument/obj-intel64/PinInstrument.so  -b $(MERGED_DUMP) -o $(DECISION) -- ./$(TEST_EXE)
	

# generate the bitcode of each source code and inject annotator function call
$(EXTERNAL_OBJ): %.o: %.cpp
	$(CXX) $(PREOPT_CXXFLAGS) $(INCLUDE) -o $@ -c $<
	$(OPT) -load $(ANALYSIS_BUILD_DIR)/libAnnotatorInjection.so -AnnotatorInjection $@ -o $@

# generate the annotator function itself
$(ANNOTATOR_BC): cmake
	$(ANALYSIS_BUILD_DIR)/AnnotatorGeneration.exe

$(OFFLOADER_BC): ../LLVMAnalysis/$(OFFLOADER_SRC) cmake
	$(CXX) $(POSTOPT_CXXFLAGS) $(INCLUDE) -o $@ -c $< $(LDFLAGS)


clean:
	rm -rf *.o *.exe *.bc *.ll *.out *.log
	rm -rf $(BUILD_DIR)
	make clean -C ../PinInstrument/

.PHONY: all bitcode cmake pin clean
