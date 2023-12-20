add_llvm_executable(mta mta.cpp LockResultValidator.cpp MTAResultValidator.cpp MTAAnnotator.cpp)
target_link_libraries(mta PUBLIC ${llvm_libs} SvfLLVM)
