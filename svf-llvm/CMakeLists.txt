# SVF-LLVM containes LLVM Libs
file(GLOB SVFLLVM_SOURCES lib/*.cpp)

add_llvm_library(SvfLLVM STATIC ${SVFLLVM_SOURCES})
target_include_directories(SvfLLVM SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(SvfLLVM PUBLIC include)
target_link_libraries(SvfLLVM PUBLIC ${Z3_LIBRARIES} SvfCoreObj)

if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(SvfLLVM intrinsics_gen)
endif()

# Maintain backward compatibility for external projects that rely on the old Svf
# lib path ${CMAKE_BINARY_DIR}/lib/libSvf.a
add_custom_command(
    TARGET SvfLLVM
    POST_BUILD
    COMMAND
        "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:SvfLLVM>"
        "${CMAKE_BINARY_DIR}/lib/$<TARGET_FILE_PREFIX:SvfLLVM>Svf$<TARGET_FILE_SUFFIX:SvfLLVM>"
    COMMENT "Copying to output directory")
