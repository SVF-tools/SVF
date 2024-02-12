add_llvm_executable(mta mta.cpp LockResultValidator.cpp MTAResultValidator.cpp MTAAnnotator.cpp)
target_link_libraries(mta PUBLIC ${llvm_libs} SvfLLVM)
set_target_properties(mta PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
