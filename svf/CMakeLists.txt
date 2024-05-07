# Define the core library
add_library(SvfCore)
add_library(SVF::Core ALIAS SvfCore)
set_target_properties(SvfCore PROPERTIES VERSION ${SVF_VERSION} SOVERSION ${SVF_VERSION_MAJOR})

# Add the public headers as an include directory
target_include_directories(SvfCore
                           PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
                                  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                  $<INSTALL_INTERFACE:${SVF_INCLUDE_DIR}>)

# Sources and headers are defined in the subdirectories
add_subdirectory(lib)
add_subdirectory(include)

# Set the installation configuration for the SvfCore package (relative to install prefix)
install(
    TARGETS SvfCore
    EXPORT SVFTargets
    RUNTIME DESTINATION ${SVF_INSTALL_BINDIR}
    LIBRARY DESTINATION ${SVF_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${SVF_INSTALL_LIBDIR}
            FILE_SET HEADERS
            DESTINATION ${SVF_INSTALL_INCLUDEDIR})
