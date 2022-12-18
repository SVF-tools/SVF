# SVF-LLVM containes LLVM Libs
file(GLOB SVFLLVM_SOURCES lib/*.cpp)

add_llvm_library(SvfLLVM STATIC ${SVFLLVM_SOURCES})
target_include_directories(SvfLLVM SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(SvfLLVM PUBLIC include)
target_link_libraries(SvfLLVM PUBLIC ${Z3_LIBRARIES} SvfCoreObj)

if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(SvfLLVM intrinsics_gen)
endif()
