set(SOURCES
    MemPartition.cpp
    MemRegion.cpp
    MemSSA.cpp
    SVFGBuilder.cpp
    SVFG.cpp 
    SVFGOPT.cpp
    SVFGStat.cpp)


add_llvm_loadable_module(Mssa ${SOURCES})
add_llvm_Library(LLVMMssa ${SOURCES})

add_dependencies(Mssa intrinsics_gen)
add_dependencies(LLVMMssa intrinsics_gen)
