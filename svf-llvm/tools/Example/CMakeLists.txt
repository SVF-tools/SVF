add_llvm_executable(svf-ex svf-ex.cpp)
target_link_libraries(svf-ex PUBLIC ${llvm_libs} SvfLLVM)
set_target_properties(svf-ex PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
