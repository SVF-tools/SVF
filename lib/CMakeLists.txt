add_subdirectory(CUDD)

# Due to a mutual dependencies, all the sub projects of the SVG are merged here
# Otherwise it is impossible to load the dependencies in opt
# NOTE: if the SVF should be linked into opt, we should probably use the individual sub projects here, rather than the combined project 
set(SOURCES
    Util/SVFUtil.cpp
    Util/CPPUtil.cpp
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
    Util/SVFModule.cpp
    Util/ICFG.cpp
    Util/VFG.cpp
    MemoryModel/ConsG.cpp
    MemoryModel/LocationSet.cpp
    MemoryModel/LocMemModel.cpp
    MemoryModel/MemModel.cpp
    MemoryModel/OfflineConsG.cpp
    MemoryModel/ExternalPAG.cpp
    MemoryModel/PAGBuilder.cpp
    MemoryModel/PAGBuilderFromFile.cpp
    MemoryModel/PAG.cpp
    MemoryModel/CHA.cpp
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
#    DDA/ContextDDA.cpp
#    DDA/DDAClient.cpp
#    DDA/DDAPass.cpp
#    DDA/DDAStat.cpp
#    DDA/FlowDDA.cpp
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
target_link_libraries(LLVMSvf ${llvm_libs})
if ( CMAKE_SYSTEM_NAME MATCHES "Darwin")
    target_link_libraries(Svf LLVMCudd ${llvm_libs})
else()
    target_link_libraries(Svf ${llvm_libs})
endif()

if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(Svf intrinsics_gen)
    add_dependencies(LLVMSvf intrinsics_gen)
endif()
