
set(LLVM_LINK_COMPONENTS BitWriter Core IPO IPA IrReader InstCombine Instrumentation Target Linker Analysis ScalarOpts Support Svf Cudd)
add_llvm_tool( wpa wpa.cpp )


