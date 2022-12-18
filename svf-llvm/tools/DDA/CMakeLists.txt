if(DEFINED IN_SOURCE_BUILD)
  add_llvm_tool(dvf dda.cpp)
else()
  add_executable(dvf dda.cpp)

  target_link_libraries(dvf SvfLLVM ${llvm_libs})

  set_target_properties(dvf PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                       ${CMAKE_BINARY_DIR}/bin)
endif()
