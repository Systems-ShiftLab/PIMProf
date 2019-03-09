CXX          := clang++-7
OPT          := opt-7
LLC          := llc-7
LINK         := llvm-link-7
CXXFLAGS     := -flto -fPIC -std=c++11 #-pedantic-errors -Wall -Wextra -Werror
PASSFLAGS    := -Xclang -load -Xclang ../build/LLVMAnalysis/libAnnotatorInjection.so
LDFLAGS      := -L/usr/lib -lstdc++ -lm -pthread #-lbacktrace -ldl
INCLUDE      :=

ANNOTATOR_DIR := ../LLVMAnalysis
ANNOTATOR_BC  := PIMProfAnnotator.bc

EXTERNAL_SRC  := print4.cpp test.cpp
EXTERNAL_OBJ  := $(EXTERNAL_SRC:%.cpp=%.o)
EXTERNAL_BC   := $(EXTERNAL_SRC:%.cpp=%.bc)

MERGED_BC     := merged.bc
OPTIMIZED_BC  := optimized.bc

# execute "make cmake" first then "make"

all: $(ANNOTATOR_BC) $(EXTERNAL_OBJ)
	$(LINK) -o $(MERGED_BC) $(ANNOTATOR_BC) $(EXTERNAL_OBJ)
	$(OPT) -load ../build/LLVMAnalysis/libAnnotatorInjection.so -AnnotatorInjection $(MERGED_BC) -o $(OPTIMIZED_BC)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o test.exe $(OPTIMIZED_BC) $(LDFLAGS)

cmake:
	cd .. && mkdir -p build
	cd ../build/ && cmake ..
	make -C ../build/

$(EXTERNAL_OBJ): %.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ -c $<

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
	rm -rf *.o *.exe *.bc *.ll
	rm -rf ../build
