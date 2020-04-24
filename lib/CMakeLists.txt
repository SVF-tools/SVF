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
add_llvm_library(LLVMSvf ${SOURCES})

link_directories( ${CMAKE_BINARY_DIR}/lib/Cudd )
llvm_map_components_to_libnames(llvm_libs bitwriter core ipo irreader instcombine instrumentation target linker analysis scalaropts support )
target_link_libraries(LLVMSvf )
if ( CMAKE_SYSTEM_NAME MATCHES "Darwin")
    target_link_libraries(Svf LLVMCudd )
else()
    target_link_libraries(Svf )
endif()

if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(Svf intrinsics_gen)
    add_dependencies(LLVMSvf intrinsics_gen)
endif()
