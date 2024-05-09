# Define the core library
add_library(SvfCore)
set_target_properties(SvfCore PROPERTIES VERSION "${SVF_VERSION}" SOVERSION "${SVF_VERSION_MAJOR}")

# Add the public headers as an include directory
target_include_directories(SvfCore PUBLIC "$<BUILD_INTERFACE:${SVF_CORE_INCLUDES}>"
                                          "$<INSTALL_INTERFACE:${SVF_INSTALL_INCLUDEDIR}>")

target_link_libraries(SvfCore PUBLIC z3::libz3)

# Ensure that the build artifacts are placed into lib/bin/include relative to the top of the build directory
set_target_properties(
    SvfCore
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}"
               ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}"
               RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")

# Sources and headers are defined in the subdirectories
add_subdirectory(lib)
add_subdirectory(include)

# Set the installation configuration for the SvfCore package (relative to install prefix)
install(
    TARGETS SvfCore
    EXPORT SVF_EXPORTED_TARGETS
    RUNTIME DESTINATION "${SVF_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${SVF_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${SVF_INSTALL_LIBDIR}"
            FILE_SET HEADERS
            DESTINATION "${SVF_INSTALL_INCLUDEDIR}")
