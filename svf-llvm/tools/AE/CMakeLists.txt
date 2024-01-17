add_llvm_executable(symabs symabs.cpp)
target_link_libraries(symabs PUBLIC ${llvm_libs} SvfLLVM)
