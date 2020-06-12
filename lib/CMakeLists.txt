add_subdirectory(CUDD)

# Due to a mutual dependencies, all the sub projects of the SVG are merged here
# Otherwise it is impossible to load the dependencies in opt
# NOTE: if the SVF should be linked into opt, we should probably use the individual sub projects here, rather than the combined project 
set(SOURCES
    SVF-FE/LLVMUtil.cpp
    SVF-FE/CPPUtil.cpp
    SVF-FE/DataFlowUtil.cpp
    SVF-FE/BreakConstantExpr.cpp
    SVF-FE/LLVMModule.cpp
    SVF-FE/CallGraphBuilder.cpp
    SVF-FE/ICFGBuilder.cpp
    SVF-FE/SymbolTableInfo.cpp
    SVF-FE/PAGBuilder.cpp
    SVF-FE/CHG.cpp
    SVF-FE/DCHG.cpp
    SVF-FE/Graph2Json.cpp
    Graphs/PTACallGraph.cpp
    Graphs/ThreadCallGraph.cpp
    Graphs/PAG.cpp
    Graphs/ICFG.cpp
    Graphs/ConsG.cpp
    Graphs/VFG.cpp
    Graphs/SVFG.cpp
    Graphs/OfflineConsG.cpp
    Graphs/ExternalPAG.cpp
    Graphs/SVFGOPT.cpp
    Graphs/SVFGStat.cpp    
    Util/SVFUtil.cpp
    Util/Conditions.cpp
    Util/ExtAPI.cpp
    Util/PathCondAllocator.cpp
    Util/PTAStat.cpp
    Util/ThreadAPI.cpp
    Util/TypeBasedHeapCloning.cpp
    MemoryModel/MemModel.cpp
    MemoryModel/LocationSet.cpp
    MemoryModel/PAGBuilderFromFile.cpp
    MemoryModel/PointerAnalysis.cpp
    MemoryModel/PointerAnalysisImpl.cpp    
    MSSA/MemPartition.cpp
    MSSA/MemRegion.cpp
    MSSA/MemSSA.cpp
    MSSA/SVFGBuilder.cpp
    WPA/Andersen.cpp
    WPA/AndersenLCD.cpp
    WPA/AndersenHCD.cpp
    WPA/AndersenHLCD.cpp
    WPA/AndersenSCD.cpp
    WPA/AndersenSFR.cpp
    WPA/AndersenStat.cpp
    WPA/AndersenWaveDiff.cpp
    WPA/CSC.cpp
    WPA/AndersenWaveDiffWithType.cpp
    WPA/FlowSensitive.cpp
    WPA/FlowSensitiveStat.cpp
    WPA/FlowSensitiveTBHC.cpp
    WPA/TypeAnalysis.cpp
    WPA/WPAPass.cpp
    SABER/DoubleFreeChecker.cpp
    SABER/FileChecker.cpp
    SABER/LeakChecker.cpp
    SABER/ProgSlice.cpp
    SABER/SaberAnnotator.cpp
    SABER/SaberCheckerAPI.cpp
    SABER/SaberSVFGBuilder.cpp
    SABER/SrcSnkDDA.cpp
    DDA/ContextDDA.cpp
    DDA/DDAClient.cpp
    DDA/DDAPass.cpp
    DDA/DDAStat.cpp
    DDA/FlowDDA.cpp
#    MTA/FSMPTA.cpp
#    MTA/LockAnalysis.cpp
#    MTA/MHP.cpp
#    MTA/MTAAnnotator.cpp
#    MTA/MTA.cpp
#    MTA/MTAResultValidator.cpp
#    MTA/MTAStat.cpp
#    MTA/PCG.cpp
#    MTA/TCT.cpp
    )

add_llvm_library(Svf ${SOURCES})

link_directories( ${CMAKE_BINARY_DIR}/lib/Cudd )
llvm_map_components_to_libnames(llvm_libs bitwriter core ipo irreader instcombine instrumentation target linker analysis scalaropts support )
target_link_libraries(Cudd)


if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(Svf intrinsics_gen)
endif()
if (UNIX AND NOT APPLE)
    add_library(svf_ubuntu STATIC ${SOURCES})
elseif (APPLE)
    add_library(svf_mac STATIC ${SOURCES})
endif()