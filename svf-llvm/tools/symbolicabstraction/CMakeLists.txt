if(DEFINED IN_SOURCE_BUILD)
    add_llvm_tool(symbolicabstraction symbolicabstraction.cpp)
else()
    add_executable(symbolicabstraction symbolicabstraction.cpp)

    target_link_libraries(symbolicabstraction SvfLLVM ${llvm_libs})

    set_target_properties(symbolicabstraction PROPERTIES RUNTIME_OUTPUT_DIRECTORY
            ${CMAKE_BINARY_DIR}/bin)
endif()