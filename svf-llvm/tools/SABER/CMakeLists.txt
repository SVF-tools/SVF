if(DEFINED IN_SOURCE_BUILD)
  add_llvm_tool(saber saber.cpp)
else()
  add_executable(saber saber.cpp)

  target_link_libraries(saber SvfLLVM ${llvm_libs})

  set_target_properties(saber PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                         ${CMAKE_BINARY_DIR}/bin)
endif()
