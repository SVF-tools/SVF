add_llvm_executable(ae2 ae_icfg.cpp)
target_link_libraries(ae2 PUBLIC ${llvm_libs} SvfLLVM)

