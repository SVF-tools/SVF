if(DEFINED IN_SOURCE_BUILD)
  add_llvm_tool(llvm2svf llvm2svf.cpp)
else()
  add_executable(llvm2svf llvm2svf.cpp)

  target_link_libraries(llvm2svf SvfLLVM ${llvm_libs})

  set_target_properties(llvm2svf PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                       ${CMAKE_BINARY_DIR}/bin)
endif()