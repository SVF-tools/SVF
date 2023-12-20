# SVF-LLVM contains LLVM Libs
file(GLOB SVFLLVM_SOURCES lib/*.cpp)

add_llvm_library(SvfLLVM ${SVFLLVM_SOURCES})

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_include_directories(SvfLLVM SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
target_link_directories(SvfLLVM PUBLIC ${LLVM_LIBRARY_DIRS})
target_compile_definitions(SvfLLVM PUBLIC ${LLVM_DEFINITIONS})

target_include_directories(SvfLLVM PUBLIC include)
target_link_libraries(SvfLLVM PUBLIC ${llvm_libs} ${Z3_LIBRARIES} SvfCore)

if(DEFINED IN_SOURCE_BUILD)
  add_dependencies(SvfLLVM intrinsics_gen)
endif()

add_subdirectory(tools)
