add_llvm_executable(ae ae.cpp)
target_link_libraries(ae PUBLIC ${llvm_libs} SvfLLVM)
set_target_properties(ae PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
