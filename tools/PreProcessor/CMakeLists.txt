
if(DEFINED IN_SOURCE_BUILD)
    set(LLVM_LINK_COMPONENTS BitWriter Core IPO IrReader InstCombine Instrumentation Target Linker Analysis ScalarOpts Support Svf Cudd)
    add_llvm_tool( pre-processor pre-processor.cpp )
else()
    add_executable( pre-processor pre-processor.cpp )

    target_link_libraries( pre-processor Svf Cudd ${llvm_libs} )

    set_target_properties( pre-processor PROPERTIES
                           RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
endif()
