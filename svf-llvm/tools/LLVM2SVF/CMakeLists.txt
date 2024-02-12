add_llvm_executable(llvm2svf llvm2svf.cpp)
target_link_libraries(llvm2svf PUBLIC ${llvm_libs} SvfLLVM)
set_target_properties(llvm2svf PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
