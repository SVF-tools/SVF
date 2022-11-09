
if(DEFINED IN_SOURCE_BUILD)
set(LLVM_LINK_COMPONENTS BitWriter Core IPO IrReader InstCombine Instrumentation Target Linker Analysis ScalarOpts Support Svf)
add_llvm_tool( summary summary.cpp )
else()
add_executable( summary summary.cpp )

target_link_libraries( summary Svf ${llvm_libs} )

set_target_properties( summary PROPERTIES
                       RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
endif()
