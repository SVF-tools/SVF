# SVF-LLVM containes LLVM Libs
file (GLOB SVF_LLVM_SOURCES lib/*.cpp)

add_llvm_library(Svf STATIC ${SVF_LLVM_SOURCES})
target_include_directories(Svf SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(Svf PUBLIC ./include)
target_link_libraries(Svf PUBLIC ${Z3_LIBRARIES} SvfCoreObj)

if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(Svf intrinsics_gen)
endif()
