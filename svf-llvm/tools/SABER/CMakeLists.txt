add_llvm_executable(saber saber.cpp)
target_link_libraries(saber PUBLIC ${llvm_libs} SvfLLVM)