
if(DEFINED IN_SOURCE_BUILD)
    set(LLVM_LINK_COMPONENTS BitWriter Core IPO IrReader InstCombine Instrumentation Target Linker Analysis ScalarOpts Support Svf Cudd)
    add_llvm_tool( svf-ex svf-ex.cpp )
else()
    add_executable( svf-ex svf-ex.cpp )

    target_link_libraries( svf-ex Svf Cudd ${llvm_libs} )

    set_target_properties( svf-ex PROPERTIES
                           RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
endif()
