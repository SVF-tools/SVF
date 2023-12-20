add_llvm_executable(dvf dda.cpp)
target_link_libraries(dvf PUBLIC SvfLLVM)
