if(DEFINED IN_SOURCE_BUILD)
  add_llvm_tool(svf-ex svf-ex.cpp)
else()
  add_executable(svf-ex svf-ex.cpp)

  target_link_libraries(svf-ex SvfLLVM ${llvm_libs})

  set_target_properties(svf-ex PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                          ${CMAKE_BINARY_DIR}/bin)
endif()
