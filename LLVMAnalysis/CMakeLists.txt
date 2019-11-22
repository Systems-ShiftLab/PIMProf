set(ANNOTATIONINJECTION "AnnotationInjection")
set(ANNOTATIONINJECTION_SRC "${ANNOTATIONINJECTION}.cpp")
add_library(${ANNOTATIONINJECTION} SHARED
    ${ANNOTATIONINJECTION_SRC} "MurmurHash3.cpp")
target_compile_features(${ANNOTATIONINJECTION} PRIVATE
    cxx_range_for cxx_auto_type)
set_target_properties(${ANNOTATIONINJECTION} PROPERTIES
    COMPILE_FLAGS "-fno-rtti -fPIC")

set(OFFLOADERINJECTION "OffloaderInjection")
set(OFFLOADERINJECTION_SRC "${OFFLOADERINJECTION}.cpp")
add_library(${OFFLOADERINJECTION} SHARED
    ${OFFLOADERINJECTION_SRC} "MurmurHash3.cpp")
target_compile_features(${OFFLOADERINJECTION} PRIVATE
    cxx_range_for cxx_auto_type)
set_target_properties(${OFFLOADERINJECTION} PROPERTIES
    COMPILE_FLAGS "-fno-rtti -fPIC")

set(ANNOTATIONGENERATION "AnnotationGeneration")
set(ANNOTATIONGENERATION_SRC "${ANNOTATIONGENERATION}.cpp")
set(ANNOTATIONGENERATION_EXE "${ANNOTATIONGENERATION}.exe")
add_executable(${ANNOTATIONGENERATION_EXE}
    ${ANNOTATIONGENERATION_SRC})
target_link_libraries(${ANNOTATIONGENERATION_EXE} LLVM)
target_compile_features(${ANNOTATIONGENERATION_EXE} PRIVATE
    cxx_range_for cxx_auto_type)
set_target_properties(${ANNOTATIONGENERATION_EXE} PROPERTIES
    COMPILE_FLAGS "-fno-rtti -fPIC")

set(OFFLOADER "PIMProfOffloader")
set(OFFLOADER_SRC "${OFFLOADER}.cpp")
add_library(${OFFLOADER} SHARED
    ${OFFLOADER_SRC})
target_compile_features(${OFFLOADER} PRIVATE
    cxx_range_for cxx_auto_type)
set_target_properties(${OFFLOADER} PROPERTIES
    COMPILE_FLAGS "-fno-rtti")
