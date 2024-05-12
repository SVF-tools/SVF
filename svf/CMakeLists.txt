# Define the core library
add_library(SvfCore)
set_target_properties(SvfCore PROPERTIES VERSION ${SVF_VERSION} SOVERSION ${SVF_VERSION_MAJOR})

# Add the public headers as an include directory
target_include_directories(SvfCore PRIVATE ${PROJECT_BINARY_DIR}/include)
target_include_directories(SvfCore PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                          $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/SVF>)

target_link_libraries(SvfCore PUBLIC ${Z3_LIBRARIES})

# Ensure that the build artifacts are placed into lib/bin/include relative to the top of the build directory
set_target_properties(
    SvfCore
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
               LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib
               ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

# Sources and headers are defined in the subdirectories
add_subdirectory(include)
add_subdirectory(lib)

# Set the installation configuration for the SvfCore package (relative to install prefix)
install(
    TARGETS SvfCore
    EXPORT SVF_EXPORTED_TARGETS
    LIBRARY DESTINATION ${SVF_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${SVF_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${SVF_INSTALL_BINDIR}
            FILE_SET HEADERS
            DESTINATION ${SVF_INSTALL_INCLUDEDIR})
