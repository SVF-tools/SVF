if(DEFINED IN_SOURCE_BUILD)
  add_llvm_tool(cfl cfl.cpp)
else()
  add_executable(cfl cfl.cpp)

  separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
  target_include_directories(cfl PUBLIC include)
  target_include_directories(cfl SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
  target_link_directories(cfl PUBLIC ${LLVM_LIBRARY_DIRS})
  target_compile_definitions(cfl PUBLIC ${LLVM_DEFINITIONS})

  target_link_libraries(cfl SvfCore SvfLLVM)

  set_target_properties(cfl PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                       ${CMAKE_BINARY_DIR}/bin)
endif()
