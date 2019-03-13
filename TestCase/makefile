CXX              := clang++-7
OPT              := opt-7
LLC              := llc-7
LINK             := llvm-link-7
DIS              := llvm-dis-7
PREOPT_CXXFLAGS  := -flto -fPIC -std=c++11 -g # -O3 -pedantic-errors -Wall -Wextra -Werror
POSTOPT_CXXFLAGS := $(PREOPT_CXXFLAGS) -fno-inline
PASSFLAGS        := -Xclang -load -Xclang ../build/LLVMAnalysis/libAnnotatorInjection.so
LDFLAGS          := 
INCLUDE          :=

ANNOTATOR_DIR := ../LLVMAnalysis
ANNOTATOR_BC  := PIMProfAnnotator.bc

EXTERNAL_SRC  := print4.cpp test.cpp
EXTERNAL_OBJ  := $(EXTERNAL_SRC:%.cpp=%.o)
EXTERNAL_BC   := $(EXTERNAL_SRC:%.cpp=%.bc)

MERGED_BC     := merged.bc
MERGED_LL     := merged.ll

TEST_EXE      := test.exe

# Process TestCase folder
all: cmake bitcode 

# generate merged bitcode, then compile to executable
bitcode: $(ANNOTATOR_BC) $(EXTERNAL_OBJ)
	$(LINK) -o $(MERGED_BC) $(ANNOTATOR_BC) $(EXTERNAL_OBJ)
	$(DIS) $(MERGED_BC) -o $(MERGED_LL)

# generate LLVM analysis tools
cmake:
	cd .. && mkdir -p build
	cd ../build/ && cmake ..
	make -C ../build/


# test pintool
# $(OPT) -O$$LEVEL $(MERGED_BC) -o merged$$LEVEL.bc; \
# $(DIS) merged$$LEVEL.bc -o merged$$LEVEL.ll; \
# $(CXX) $(POSTOPT_CXXFLAGS) $(INCLUDE) -o test$$LEVEL.exe merged$$LEVEL.bc $(LDFLAGS); \
#
# cp ./merged.bc ./merged$$LEVEL.bc; \
# $(CXX) $(POSTOPT_CXXFLAGS) -O$$LEVEL $(INCLUDE) -o test$$LEVEL.exe merged$$LEVEL.bc $(LDFLAGS); \
#

pin: all
	make -C ../PinInstrument/
	for LEVEL in 0 1 2 3 ; do \
		cp ./merged.bc ./merged$$LEVEL.bc; \
		$(CXX) $(POSTOPT_CXXFLAGS) -O$$LEVEL $(INCLUDE) -o test$$LEVEL.exe merged$$LEVEL.bc $(LDFLAGS); \
		objdump -d test$$LEVEL.exe > objdump$$LEVEL.out; \
		$(PIN_ROOT)/pin -t ../PinInstrument/obj-intel64/PinInstrument.so -- ./test$$LEVEL.exe -o opcodemix.out > output$$LEVEL.out; \
	done
	

# generate the bitcode of each source code and apply annotator injection
$(EXTERNAL_OBJ): %.o: %.cpp
	$(CXX) $(PREOPT_CXXFLAGS) $(INCLUDE) -o $@ -c $<
	$(OPT) -load ../build/LLVMAnalysis/libAnnotatorInjection.so -AnnotatorInjection $@ -o $@

$(ANNOTATOR_BC):
	../build/LLVMAnalysis/AnnotatorGeneration.exe
	# cp $(ANNOTATOR_BC) $(ANNOTATOR_OBJ)
	# $(LLC) -filetype=obj -o $(ANNOTATOR_OBJ) $(ANNOTATOR_BC)

# ll: $(ANNOTATOR_LL) $(EXTERNAL_LL)

# $(EXTERNAL_LL) : %.ll : %.cpp
# 	$(CXX) $(PASSFLAGS) $(INCLUDE) -emit-llvm -o $@ -S $<

# $(ANNOTATOR_LL) : %.ll : $(ANNOTATOR_DIR)/%.cpp
# 	$(CXX) $(PASSFLAGS) $(INCLUDE) -emit-llvm -o $@ -S $<

# bc: $(ANNOTATOR_BC) $(EXTERNAL_BC)

# $(EXTERNAL_BC) : %.bc : %.cpp
# 	$(CXX) $(PASSFLAGS) $(INCLUDE) -emit-llvm -o $@ -c $<

# $(ANNOTATOR_BC) : %.bc : $(ANNOTATOR_DIR)/%.cpp
# 	$(CXX) $(PASSFLAGS) $(INCLUDE) -emit-llvm -o $@ -c $<

clean:
	rm -rf *.o *.exe *.bc *.ll *.out *.log
	rm -rf ../build

.PHONY: all bitcode cmake pin clean