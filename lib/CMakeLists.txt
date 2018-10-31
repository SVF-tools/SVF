add_subdirectory(CUDD)

# Due to a mutual dependencies, all the sub projects of the SVG are merged here
# Otherwise it is impossible to load the dependencies in opt
# NOTE: if the SVF should be linked into opt, we should probably use the individual sub projects here, rather than the combined project 
set(SOURCES
    Util/AnalysisUtil.cpp
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
    MemoryModel/ConsG.cpp
    MemoryModel/LocationSet.cpp
    MemoryModel/LocMemModel.cpp
    MemoryModel/MemModel.cpp
    MemoryModel/PAGBuilder.cpp
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
    WPA/AndersenStat.cpp
    WPA/AndersenWave.cpp
    WPA/AndersenWaveDiff.cpp
    WPA/AndersenWaveDiffWithType.cpp
    WPA/FlowSensitive.cpp
    WPA/FlowSensitiveStat.cpp
    WPA/TypeAnalysis.cpp
    WPA/WPAPass.cpp)

add_llvm_loadable_module(Svf ${SOURCES})
add_llvm_Library(LLVMSvf ${SOURCES})

link_directories( ${CMAKE_BINARY_DIR}/lib/Cudd )

## On Linux platform, we don't link LLVM libs for static/dynamic libraries since we rely on target linkage
## For Mac, if we don't link we may face errors like "ld: symbol(s) not found for architecture x86_64"
## need further inspection on this
if ( CMAKE_SYSTEM_NAME MATCHES "Darwin")
    llvm_map_components_to_libnames(llvm_libs bitwriter core ipo irreader instcombine instrumentation target linker analysis scalaropts support )
    target_link_libraries(LLVMSvf ${llvm_libs})
    target_link_libraries(Svf LLVMCudd ${llvm_libs})
else()
    target_link_libraries(Svf)
endif()
set_property(TARGET LLVMSvf PROPERTY POSITION_INDEPENDENT_CODE ON)

if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(Svf intrinsics_gen)
    add_dependencies(LLVMSvf intrinsics_gen)
endif()
