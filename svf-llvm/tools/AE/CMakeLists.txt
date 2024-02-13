add_llvm_executable(ae ae.cpp)
target_link_libraries(ae PUBLIC ${llvm_libs} SvfLLVM)