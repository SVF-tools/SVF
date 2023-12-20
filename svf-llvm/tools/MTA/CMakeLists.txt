if(DEFINED IN_SOURCE_BUILD)
  add_llvm_tool(mta mta.cpp LockResultValidator.cpp MTAResultValidator.cpp
                MTAAnnotator.cpp)
else()
  add_executable(mta mta.cpp LockResultValidator.cpp MTAResultValidator.cpp
                     MTAAnnotator.cpp)

  target_link_libraries(mta SvfCore SvfLLVM)

  separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
  target_include_directories(mta PUBLIC include)
  target_include_directories(mta SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
  target_link_directories(mta PUBLIC ${LLVM_LIBRARY_DIRS})
  target_compile_definitions(mta PUBLIC ${LLVM_DEFINITIONS})

  set_target_properties(mta PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                       ${CMAKE_BINARY_DIR}/bin)
endif()
