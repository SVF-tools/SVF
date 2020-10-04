
if(DEFINED IN_SOURCE_BUILD)
    set(LLVM_LINK_COMPONENTS BitWriter Core IPO IrReader InstCombine Instrumentation Target Linker Analysis ScalarOpts Support Svf Cudd)
    add_llvm_tool( wpa wpa.cpp )
else()
    if (LLVM_LINK_LLVM_DYLIB)
        set(llvm_libs LLVM)
    else()
        llvm_map_components_to_libnames(llvm_libs bitwriter core ipo irreader instcombine instrumentation target linker analysis scalaropts support transformutils)
    endif()
    add_executable( wpa wpa.cpp )

    target_link_libraries( wpa Svf Cudd ${llvm_libs} )

    set_target_properties( wpa PROPERTIES
                           RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
endif()


