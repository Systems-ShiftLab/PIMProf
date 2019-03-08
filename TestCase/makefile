CXX          := clang++-7
LLC          := llc-7
LINK         := llvm-link-7
CXXFLAGS     := -flto -fPIC -std=c++11 #-pedantic-errors -Wall -Wextra -Werror
PASSFLAGS    := -Xclang -load -Xclang ../build/LLVMAnalysis/libAnnotatorInjection.so
LDFLAGS      := -L/usr/lib -lstdc++ -lm -pthread #-lbacktrace -ldl
INCLUDE      :=

ANNOTATOR_DIR := ../LLVMAnalysis

EXTERNAL_SRC  := print4.cpp test.cpp

ANNOTATOR_OBJ := PIMProfAnnotator.o
EXTERNAL_OBJ  := $(EXTERNAL_SRC:%.cpp=%.o)

EXTERNAL_LL  := $(EXTERNAL_SRC:%.cpp=%.ll)

ANNOTATOR_BC := $(ANNOTATOR_OBJ:%.o=%.bc)
EXTERNAL_BC  := $(EXTERNAL_SRC:%.cpp=%.bc)

# execute "make cmake" first then "make"

all: $(ANNOTATOR_OBJ) $(EXTERNAL_OBJ)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o test.exe $^ $(LDFLAGS)
	$(LINK) -o big.bc $(ANNOTATOR_BC) $(EXTERNAL_OBJ)

cmake:
	cd .. && mkdir -p build
	cd ../build/ && cmake ..
	make -C ../build/

$(EXTERNAL_OBJ): %.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ -c $<

$(ANNOTATOR_OBJ):
	../build/LLVMAnalysis/AnnotatorGeneration.exe
	# cp $(ANNOTATOR_BC) $(ANNOTATOR_OBJ)
	$(LLC) -filetype=obj -o $(ANNOTATOR_OBJ) $(ANNOTATOR_BC)

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
