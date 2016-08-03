set(SOURCES
    Andersen.cpp
    AndersenLCD.cpp
    AndersenStat.cpp
    AndersenWave.cpp
    AndersenWaveDiff.cpp
    FlowSensitive.cpp
    FlowSensitiveStat.cpp
    WPAPass.cpp)

add_llvm_loadable_module(Wpa ${SOURCES})
add_llvm_Library(LLVMWpa ${SOURCES})

add_dependencies(Wpa intrinsics_gen)
add_dependencies(LLVMWpa intrinsics_gen)
