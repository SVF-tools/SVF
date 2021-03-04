add_subdirectory(CUDD)

# Due to a mutual dependencies, all the sub projects of the SVG are merged here
# Otherwise it is impossible to load the dependencies in opt
# NOTE: if the SVF should be linked into opt, we should probably use the individual sub projects here, rather than the combined project 


file (GLOB SOURCES
        SVF-FE/*.cpp
        Graphs/*.cpp
        Util/*.cpp
        MemoryModel/*.cpp
        MSSA/*.cpp
        WPA/*.cpp
        SABER/*.cpp
        DDA/*.cpp)
add_llvm_library(Svf STATIC ${SOURCES} LINK_LIBS Cudd)

link_directories( ${CMAKE_BINARY_DIR}/lib/Cudd )


if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(Svf intrinsics_gen)
endif()
