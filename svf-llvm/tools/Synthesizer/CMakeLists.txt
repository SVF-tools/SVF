add_llvm_executable(synthesizer synthesizer.cpp)
target_link_libraries(synthesizer PUBLIC ${llvm_libs} SvfLLVM)
