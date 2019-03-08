add_library(AnnotatorInjection MODULE
    AnnotatorInjection.cpp
)

add_executable(AnnotatorGeneration.exe
    AnnotatorGeneration.cpp
)

target_compile_features(AnnotatorInjection PRIVATE cxx_range_for cxx_auto_type)

set_target_properties(AnnotatorInjection PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
)

target_compile_features(AnnotatorGeneration.exe PRIVATE cxx_range_for cxx_auto_type)

set_target_properties(AnnotatorGeneration.exe PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
)

target_link_libraries(AnnotatorGeneration.exe
    LLVM
)
