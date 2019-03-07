add_library(LLVMInjection MODULE
    LLVMInjection.cpp
)

add_executable(AnnotatorGeneration
    AnnotatorGeneration.cpp
)

target_compile_features(LLVMInjection PRIVATE cxx_range_for cxx_auto_type)

set_target_properties(LLVMInjection PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
)

if(APPLE)
    set_target_properties(LLVMInjection PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif(APPLE)
