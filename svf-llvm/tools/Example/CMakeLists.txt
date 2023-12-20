if(DEFINED IN_SOURCE_BUILD)
  add_llvm_tool(svf-ex svf-ex.cpp)
else()
  add_executable(svf-ex svf-ex.cpp)

  target_link_libraries(svf-ex SvfCore SvfLLVM)

  separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
  target_include_directories(svf-ex PUBLIC include)
  target_include_directories(svf-ex SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
  target_link_directories(svf-ex PUBLIC ${LLVM_LIBRARY_DIRS})
  target_compile_definitions(svf-ex PUBLIC ${LLVM_DEFINITIONS})

  set_target_properties(svf-ex PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                          ${CMAKE_BINARY_DIR}/bin)
endif()
