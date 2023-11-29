if(DEFINED IN_SOURCE_BUILD)
    add_llvm_tool(testSA testSA.cpp)
else()
    add_executable(testSA testSA.cpp)

    target_link_libraries(testSA SvfLLVM ${llvm_libs})

    set_target_properties(testSA PROPERTIES RUNTIME_OUTPUT_DIRECTORY
            ${CMAKE_BINARY_DIR}/bin)
endif()