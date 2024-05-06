# Grab all .cpp source files in this directory and add them to the SvfLLVM package
file(GLOB_RECURSE SVF_LLVM_SOURCES *.cpp)
target_sources(SvfLLVM PRIVATE ${SVF_LLVM_SOURCES})

# Add the compile command to generate extapi.bc from the extapi.c source file
include(GenExtAPI.cmake)

# Ensure SvfLLVM depends on extapi.bc to avoid having to build all when building individual tools
add_dependencies(SvfLLVM gen_extapi_ir)
