add_subdirectory(SABER)
add_subdirectory(WPA)
add_subdirectory(Example)
add_subdirectory(DDA)
add_subdirectory(MTA)
add_subdirectory(CFL)
add_subdirectory(LLVM2SVF)

# Build SvfLLVM tools with C++ standard C++17
set_property(TARGET cfl dvf svf-ex llvm2svf mta saber wpa PROPERTY CXX_STANDARD 17)
set_property(TARGET cfl dvf svf-ex llvm2svf mta saber wpa PROPERTY CXX_EXTENSIONS ON)
set_property(TARGET cfl dvf svf-ex llvm2svf mta saber wpa PROPERTY CXX_STANDARD_REQUIRED ON)
