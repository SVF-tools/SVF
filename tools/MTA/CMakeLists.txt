
if(DEFINED IN_SOURCE_BUILD)
    set(LLVM_LINK_COMPONENTS BitWriter Core IPO IrReader InstCombine Instrumentation Target Linker Analysis ScalarOpts Support SvfLLVM Cudd)
    add_llvm_tool( mta mta.cpp )
else()
    add_executable( mta mta.cpp LockResultValidator.cpp MTAResultValidator.cpp MTAAnnotator.cpp )

    target_link_libraries( mta SvfLLVM ${llvm_libs} )

    set_target_properties( mta PROPERTIES
                           RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
endif()


