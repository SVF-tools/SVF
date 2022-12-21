if(DEFINED IN_SOURCE_BUILD)
  add_llvm_tool(cfl cfl.cpp)
else()
  add_executable(cfl cfl.cpp)

  target_link_libraries(cfl SvfLLVM ${llvm_libs})

  set_target_properties(cfl PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                       ${CMAKE_BINARY_DIR}/bin)
endif()
