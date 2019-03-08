CXX          := clang++-7
LLC          := llc-7
CXXFLAGS     := -flto -fPIC -std=c++11 -O3 #-pedantic-errors -Wall -Wextra -Werror
PASSFLAGS    := -Xclang -load -Xclang ../build/AnnotatorInjection/libAnnotatorInjection.so
LDFLAGS      := -L/usr/lib -lstdc++ -lm -pthread #-lbacktrace -ldl
INCLUDE      :=

ANNOTATOR_DIR := ../AnnotatorInjection

INJECTED_SRC  := print4.cpp test.cpp

ANNOTATOR_OBJ := PIMProfAnnotator.o
INJECTED_OBJ  := $(INJECTED_SRC:%.cpp=%.o)

INJECTED_LL  := $(INJECTED_SRC:%.cpp=%.ll)

ANNOTATOR_BC := $(ANNOTATOR_OBJ:%.o=%.bc)
INJECTED_BC  := $(INJECTED_SRC:%.cpp=%.bc)

# execute "make cmake" first then "make"

all: $(ANNOTATOR_OBJ) $(INJECTED_OBJ)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o test.exe $^ $(LDFLAGS)

cmake:
	cd .. && mkdir -p build
	cd ../build/ && cmake ..
	make -C ../build/

$(INJECTED_OBJ): %.o: %.cpp
	$(CXX) $(PASSFLAGS) $(CXXFLAGS) $(INCLUDE) -o $@ -c $<

$(ANNOTATOR_OBJ):
	../build/AnnotatorInjection/AnnotatorGeneration.exe
	$(LLC) -filetype=obj -o $(ANNOTATOR_OBJ) $(ANNOTATOR_BC)

ll: $(ANNOTATOR_LL) $(INJECTED_LL)

$(INJECTED_LL) : %.ll : %.cpp
	$(CXX) $(PASSFLAGS) $(INCLUDE) -emit-llvm -o $@ -S $<

$(ANNOTATOR_LL) : %.ll : $(ANNOTATOR_DIR)/%.cpp
	$(CXX) $(PASSFLAGS) $(INCLUDE) -emit-llvm -o $@ -S $<

bc: $(ANNOTATOR_BC) $(INJECTED_BC)

$(INJECTED_BC) : %.bc : %.cpp
	$(CXX) $(PASSFLAGS) $(INCLUDE) -emit-llvm -o $@ -c $<

$(ANNOTATOR_BC) : %.bc : $(ANNOTATOR_DIR)/%.cpp
	$(CXX) $(PASSFLAGS) $(INCLUDE) -emit-llvm -o $@ -c $<

clean:
	rm -rf *.o *.exe *.bc *.ll
	rm -rf ../build
