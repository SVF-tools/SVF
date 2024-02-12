add_llvm_executable(dvf dda.cpp)
target_link_libraries(dvf PUBLIC ${llvm_libs} SvfLLVM)
set_target_properties(dvf PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
