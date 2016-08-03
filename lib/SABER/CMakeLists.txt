set(SOURCES
    DoubleFreeChecker.cpp
    FileChecker.cpp
    LeakChecker.cpp
    Makefile  ProgSlice.cpp
    SaberAnnotator.cpp
    SaberCheckerAPI.cpp
    SaberSVFGBuilder.cpp
    SrcSnkDDA.cpp)

add_llvm_loadable_module(Saber ${SOURCES})
add_llvm_Library(LLVMSaber ${SOURCES})

add_dependencies(Saber intrinsics_gen)
add_dependencies(LLVMSaber intrinsics_gen)
