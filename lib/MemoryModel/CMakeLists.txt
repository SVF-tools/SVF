set(SOURCES
    ConsG.cpp
    LocationSet.cpp
    LocMemModel.cpp
    MemModel.cpp
    PAGBuilder.cpp
    PAG.cpp
    PointerAnalysis.cpp)


add_llvm_loadable_module(MemModel ${SOURCES})
add_llvm_Library(LLVMMemModel ${SOURCES})

add_dependencies(MemModel intrinsics_gen)
add_dependencies(LLVMMemModel intrinsics_gen)
