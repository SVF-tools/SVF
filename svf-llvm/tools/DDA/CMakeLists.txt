if(DEFINED IN_SOURCE_BUILD)
  add_llvm_tool(dvf dda.cpp)
else()
  add_executable(dvf dda.cpp)

  separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
  target_include_directories(dvf PUBLIC include)
  target_include_directories(dvf SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
  target_link_directories(dvf PUBLIC ${LLVM_LIBRARY_DIRS})
  target_compile_definitions(dvf PUBLIC ${LLVM_DEFINITIONS})

  target_link_libraries(dvf SvfCore SvfLLVM)

  set_target_properties(dvf PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                       ${CMAKE_BINARY_DIR}/bin)
endif()
