add_subdirectory(SABER)
add_subdirectory(WPA)
add_subdirectory(Example)
add_subdirectory(DDA)
add_subdirectory(MTA)
add_subdirectory(CFL)
add_subdirectory(LLVM2SVF)

# Build SvfLLVM tools with C++ standard C++17
set_target_properties(cfl dvf svf-ex llvm2svf mta saber wpa
    PROPERTIES  CXX_STANDARD 17
                CXX_EXTENSIONS ON
                CXX_STANDARD_REQUIRED ON
                RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
