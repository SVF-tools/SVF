if(DEFINED IN_SOURCE_BUILD)
  set(LLVM_LINK_COMPONENTS
      BitWriter
      Core
      IPO
      IrReader
      InstCombine
      Instrumentation
      Target
      Linker
      Analysis
      ScalarOpts
      Support
      SvfLLVM)
endif()

add_subdirectory(SABER)
add_subdirectory(WPA)
add_subdirectory(Example)
add_subdirectory(DDA)
add_subdirectory(MTA)
add_subdirectory(CFL)
add_subdirectory(LLVM2SVF)
