add_llvm_executable(cfl cfl.cpp)
target_link_libraries(cfl PUBLIC ${llvm_libs} SvfLLVM)
set_target_properties(cfl PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
