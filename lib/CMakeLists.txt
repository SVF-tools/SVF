
# Due to a mutual dependencies, all the sub projects of the SVG are merged here
# Otherwise it is impossible to load the dependencies in opt
# NOTE: if the SVF should be linked into opt, we should probably use the individual sub projects here, rather than the combined project

file (GLOB CORE_LIB_SOURCES
        AbstractExecution/*.cpp
        CFL/*.cpp
        DDA/*.cpp
        FastCluster/*.cpp
        Graphs/*.cpp
        MemoryModel/*.cpp
        MSSA/*.cpp
        MTA/*.cpp
        SABER/*.cpp
        SVFIR/*.cpp
        Util/*.cpp
        WPA/*.cpp
)

# Refer to this link for detail:
# https://stackoverflow.com/questions/68864536/cmake-linking-static-libraries-in-different-subdirectories-into-one-single-stati
add_library(SvfCoreObj OBJECT ${CORE_LIB_SOURCES})
add_library(SvfCore STATIC)
target_link_libraries(SvfCore PUBLIC ${Z3_LIBRARIES} SvfCoreObj)

# Svf containes LLVM Libs
file (GLOB LLVM_FE_SOURCES SVF-LLVM/*.cpp)

add_llvm_library(Svf STATIC ${LLVM_FE_SOURCES})
target_include_directories(Svf SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
target_link_libraries(Svf PUBLIC ${Z3_LIBRARIES} SvfCoreObj)

if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(Svf intrinsics_gen)
endif()
