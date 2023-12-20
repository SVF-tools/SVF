if(DEFINED IN_SOURCE_BUILD)
  add_llvm_tool(llvm2svf llvm2svf.cpp)
else()
  add_executable(llvm2svf llvm2svf.cpp)

  target_link_libraries(llvm2svf SvfCore SvfLLVM)

  separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
  target_include_directories(llvm2svf PUBLIC include)
  target_include_directories(llvm2svf SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
  target_link_directories(llvm2svf PUBLIC ${LLVM_LIBRARY_DIRS})
  target_compile_definitions(llvm2svf PUBLIC ${LLVM_DEFINITIONS})

  set_target_properties(llvm2svf PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                       ${CMAKE_BINARY_DIR}/bin)
endif()