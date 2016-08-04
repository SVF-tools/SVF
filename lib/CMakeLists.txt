add_subdirectory(CUDD)

# Due to a mutual dependencies, all the sub projects of the SVG are merged here
# Otherwise it is impossible to load the dependencies in opt
# NOTE: if the SVF should be linked into opt, we should probably use the individual sub projects here, rather than the combined project 
set(SOURCES
    Util/AnalysisUtil.cpp
    Util/DataFlowUtil.cpp
    Util/PTACallGraph.cpp
    Util/RaceAnnotator.cpp
    Util/ThreadCallGraph.cpp
    Util/BreakConstantExpr.cpp
    Util/Conditions.cpp
    Util/ExtAPI.cpp
    Util/PathCondAllocator.cpp
    Util/PTAStat.cpp
    Util/ThreadAPI.cpp
    MemoryModel/ConsG.cpp
    MemoryModel/LocationSet.cpp
    MemoryModel/LocMemModel.cpp
    MemoryModel/MemModel.cpp
    MemoryModel/PAGBuilder.cpp
    MemoryModel/PAG.cpp
    MemoryModel/PointerAnalysis.cpp
    MSSA/MemPartition.cpp
    MSSA/MemRegion.cpp
    MSSA/MemSSA.cpp
    MSSA/SVFGBuilder.cpp
    MSSA/SVFG.cpp 
    MSSA/SVFGOPT.cpp
    MSSA/SVFGStat.cpp
    SABER/DoubleFreeChecker.cpp
    SABER/FileChecker.cpp
    SABER/LeakChecker.cpp
    SABER/ProgSlice.cpp
    SABER/SaberAnnotator.cpp
    SABER/SaberCheckerAPI.cpp
    SABER/SaberSVFGBuilder.cpp
    SABER/SrcSnkDDA.cpp
    WPA/Andersen.cpp
    WPA/AndersenLCD.cpp
    WPA/AndersenStat.cpp
    WPA/AndersenWave.cpp
    WPA/AndersenWaveDiff.cpp
    WPA/FlowSensitive.cpp
    WPA/FlowSensitiveStat.cpp
    WPA/WPAPass.cpp)

add_llvm_loadable_module(Svf ${SOURCES})
add_llvm_Library(LLVMSvf ${SOURCES})


if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(Svf intrinsics_gen)
    add_dependencies(LLVMSvf intrinsics_gen)
endif()
