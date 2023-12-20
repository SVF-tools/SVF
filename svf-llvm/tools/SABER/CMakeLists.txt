add_llvm_executable(saber saber.cpp)
target_link_libraries(saber PUBLIC SvfLLVM)
