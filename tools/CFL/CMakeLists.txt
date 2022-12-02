
if(DEFINED IN_SOURCE_BUILD)
    set(LLVM_LINK_COMPONENTS BitWriter Core IPO IrReader InstCombine Instrumentation Target Linker Analysis ScalarOpts Support SvfLLVM)
    add_llvm_tool( cfl cfl.cpp )
else()
    add_executable( cfl cfl.cpp )

    target_link_libraries( cfl SvfLLVM ${llvm_libs} )

    set_target_properties( cfl PROPERTIES
                           RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
endif()
