# SVF-LLVM contains LLVM Libs
file(GLOB SVFLLVM_SOURCES lib/*.cpp)

add_llvm_library(SvfLLVM ${SVFLLVM_SOURCES})

# Set the LLVM header and library paths; note that this does not link the LLVM library itself but
# makes it (and the LLVM headers) available for all modules defined hereafter/in subdirectories
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

target_include_directories(SvfLLVM PUBLIC include)
target_link_libraries(SvfLLVM PUBLIC ${llvm_libs} ${Z3_LIBRARIES} SvfCore)

if(DEFINED IN_SOURCE_BUILD)
  add_dependencies(SvfLLVM intrinsics_gen)
endif()

add_subdirectory(tools)
