if(DEFINED IN_SOURCE_BUILD)
  add_llvm_tool(saber saber.cpp)
else()
  add_executable(saber saber.cpp)

  target_link_libraries(saber SvfCore SvfLLVM)

  separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
  target_include_directories(saber PUBLIC include)
  target_include_directories(saber SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
  target_link_directories(saber PUBLIC ${LLVM_LIBRARY_DIRS})
  target_compile_definitions(saber PUBLIC ${LLVM_DEFINITIONS})

  set_target_properties(saber PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                         ${CMAKE_BINARY_DIR}/bin)
endif()
