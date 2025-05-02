# Define the core library & create alias SVF::SvfCore (for scoping when linking against SVF)
add_library(SvfCore)
add_library(SVF::Core ALIAS SvfCore)

# Inherit compiler & linker options by publically linking against the interface library
target_link_libraries(SvfCore PUBLIC SvfFlags)

# Gather & set all of the core library's source files by globbing all .h and .cpp files (recursively)
file(GLOB_RECURSE SVF_CORE_HEADERS ${CMAKE_CURRENT_LIST_DIR}/include/*.h)
file(GLOB_RECURSE SVF_CORE_SOURCES ${CMAKE_CURRENT_LIST_DIR}/lib/*.cpp)
target_sources(
    SvfCore
    PUBLIC FILE_SET HEADERS
                    BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}/include
                    FILES ${SVF_CORE_HEADERS}
    PRIVATE ${SVF_CORE_SOURCES}
    )

# Set & expose the include directories for the core library
target_include_directories(
    SvfCore
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    )

# Set generic properties of the core library (not exposed)
set_target_properties(
    SvfCore
    PROPERTIES CMAKE_CXX_EXTENSIONS ON
               VERSION ${SVF_VERSION} # Set SVF version
               SOVERSION ${SVF_VERSION_MAJOR} # Set shared lib's version
               POSITION_INDEPENDENT_CODE ${SVF_USE_PIC} # Enable PIC if configured
               INTERPROCEDURAL_OPTIMIZATION ${SVF_USE_LTO} # Enable LTO if configured
               RUNTIME_OUTPUT_DIRECTORY ${SVF_BINARY_DIR}/bin # Compile/output executables to build/bin
               LIBRARY_OUTPUT_DIRECTORY ${SVF_BINARY_DIR}/lib # Compile/output shared libraries to build/lib
               ARCHIVE_OUTPUT_DIRECTORY ${SVF_BINARY_DIR}/lib # Compile/output static libraries to build/lib
    )

# Install the core library's files to the regular/configured install tree
install(
    TARGETS SvfCore
    EXPORT SVFTargets
    RUNTIME DESTINATION ${SVF_INSTALL_BINDIR}
    LIBRARY DESTINATION ${SVF_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${SVF_INSTALL_LIBDIR}
            FILE_SET HEADERS
            DESTINATION ${SVF_INSTALL_INCLUDEDIR}/SVF
    )
