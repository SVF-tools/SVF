add_llvm_executable(llvm2svf llvm2svf.cpp)
target_link_libraries(llvm2svf PUBLIC ${llvm_libs} SvfLLVM)
