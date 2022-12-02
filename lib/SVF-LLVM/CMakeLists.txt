
if (LLVM_LINK_LLVM_DYLIB)
    set(llvm_libs LLVM)
else()
    llvm_map_components_to_libnames(
        llvm_libs
        bitwriter
        core
        ipo
        irreader
        instcombine
        instrumentation
        target
        linker
        analysis
        scalaropts
        support
        transformutils)
endif()

file (GLOB LLVM_FE_SOURCES ./*.cpp)

add_llvm_library(SvfLLVM STATIC ${LLVM_FE_SOURCES} LINK_LIBS SvfCore)

if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(SvfLLVM intrinsics_gen)
endif()
