
# Due to a mutual dependencies, all the sub projects of the SVG are merged here
# Otherwise it is impossible to load the dependencies in opt
# NOTE: if the SVF should be linked into opt, we should probably use the individual sub projects here, rather than the combined project

file (GLOB SOURCES
        AbstractExecution/*.cpp
        Graphs/*.cpp
        Util/*.cpp
        SVFIR/*.cpp
        MemoryModel/*.cpp
        MSSA/*.cpp
        WPA/*.cpp
        MTA/*.cpp
        SABER/*.cpp
        DDA/*.cpp
        CFL/*.cpp
        FastCluster/*.cpp
)

add_library(SvfCore STATIC ${SOURCES})
target_link_libraries(SvfCore ${Z3_LIBRARIES})

add_subdirectory(SVF-LLVM)
