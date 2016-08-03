set(SOURCES
    AnalysisUtil.cpp
    DataFlowUtil.cpp
    PTACallGraph.cpp
    RaceAnnotator.cpp
    ThreadCallGraph.cpp
    BreakConstantExpr.cpp
    Conditions.cpp
    ExtAPI.cpp
    PathCondAllocator.cpp
    PTAStat.cpp
    ThreadAPI.cpp)


add_llvm_loadable_module(SvfUtil ${SOURCES})
add_llvm_Library(LLVMSvfUtil ${SOURCES})

add_dependencies(SvfUtil intrinsics_gen)
add_dependencies(LLVMSvfUtil intrinsics_gen)
