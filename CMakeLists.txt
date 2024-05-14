cmake_minimum_required(VERSION 3.23)

project(SVF
  VERSION 3.0
  DESCRIPTION "SVF is a static value-flow analysis tool for LLVM-based languages"
  HOMEPAGE_URL "https://github.com/SVF-tools/SVF"
  LANGUAGES C CXX
)

# =================================================================================
# Base configuration
# =================================================================================

# Ensure installation directories like ${CMAKE_INSTALL_LIBDIR} are available
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Build SVF with C++ standard C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
set(INSTALL_RPATH_USE_LINK_PATH ON)

# =================================================================================
# SVF options & settings
# =================================================================================

# Configurable (string) options for building SVF
set(SVF_SANITIZE
    ""
    CACHE STRING "Create sanitizer build (address)")

# Configurable (boolean) options for building SVF
option(SVF_COVERAGE "Create coverage build")
option(SVF_WARN_AS_ERROR "Treat warnings as errors when building SVF (default: on)" ON)
option(SVF_EXPORT_DYNAMIC "Export all (not only used) dynamic symbols to dynamic symbol table")
option(SVF_ENABLE_ASSERTIONS "Always enable assertions")
option(SVF_ENABLE_OPAQUE_POINTERS "Build ExtAPI.bc with opaque pointers enabled")

# Store the build directory for exposing it later; ensure also available in config
set(SVF_ROOT_DIR ${PROJECT_SOURCE_DIR})
set(SVF_SOURCE_DIR ${PROJECT_SOURCE_DIR})
set(SVF_BUILD_DIR ${PROJECT_BINARY_DIR})

# Set variables to denote where different SVF structures/files are located
set(SVF_CORE_ROOT ${SVF_SOURCE_DIR}/svf)
set(SVF_CORE_SRCS ${SVF_CORE_ROOT}/lib)
set(SVF_CORE_INCLUDES ${SVF_CORE_ROOT}/include)

set(SVF_LLVM_ROOT ${SVF_SOURCE_DIR}/svf-llvm)
set(SVF_LLVM_SRCS ${SVF_LLVM_ROOT}/lib)
set(SVF_LLVM_TOOLS ${SVF_LLVM_ROOT}/tools)
set(SVF_LLVM_INCLUDES ${SVF_LLVM_ROOT}/include)

# Set the installation tree structure
set(SVF_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
set(SVF_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})
set(SVF_INSTALL_PKGDIR ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
set(SVF_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/SVF)
set(SVF_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR}/SVF)

# Set where extapi.bc is compiled and installed to
set(SVF_EXTAPI_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
set(SVF_EXTAPI_INSTALL_FILE ${CMAKE_INSTALL_LIBDIR}/extapi.bc)

message(STATUS "Building SVF with configuration:
        SVF version:                                ${SVF_VERSION}
        SVF option - create sanitiser build (str):  ${SVF_SANITIZE}
        SVF option - coverage build:                ${SVF_COVERAGE}
        SVF option - warnings as errors:            ${SVF_WARN_AS_ERROR}
        SVF option - unused dynamic symbols:        ${SVF_EXPORT_DYNAMIC}
        SVF option - enable build assertions:       ${SVF_ENABLE_ASSERTIONS}
        SVF option - build opaque pointers:         ${SVF_ENABLE_OPAQUE_POINTERS}")

message(STATUS "SVF installation configuration:
        SVF_SOURCE_DIR:             ${SVF_SOURCE_DIR}
        SVF_BUILD_DIR:              ${SVF_BUILD_DIR}

        SVF_CORE_ROOT:              ${SVF_CORE_ROOT}
        SVF_CORE_SRCS:              ${SVF_CORE_SRCS}
        SVF_CORE_INCLUDES:          ${SVF_CORE_INCLUDES}

        SVF_LLVM_ROOT:              ${SVF_LLVM_ROOT}
        SVF_LLVM_SRCS:              ${SVF_LLVM_SRCS}
        SVF_LLVM_TOOLS:             ${SVF_LLVM_TOOLS}
        SVF_LLVM_INCLUDES:          ${SVF_LLVM_INCLUDES}

        SVF_INSTALL_BINDIR:         ${SVF_INSTALL_BINDIR}
        SVF_INSTALL_LIBDIR:         ${SVF_INSTALL_LIBDIR}
        SVF_INSTALL_CMAKEDIR:       ${SVF_INSTALL_CMAKEDIR}
        SVF_INSTALL_INCLUDEDIR:     ${SVF_INSTALL_INCLUDEDIR}

        SVF_EXTAPI_INSTALL_DIR:     ${SVF_EXTAPI_INSTALL_DIR}
        SVF_EXTAPI_INSTALL_FILE:    ${SVF_EXTAPI_INSTALL_FILE}
")

message(STATUS "Using CMake build configuration:
        Project source dir:                         ${PROJECT_SOURCE_DIR}
        Project binary dir:                         ${PROJECT_BINARY_DIR}
        CMake generator:                            ${CMAKE_GENERATOR}
        CMake C compiler:                           ${CMAKE_C_COMPILER_ID}
        CMake C++ compiler:                         ${CMAKE_CXX_COMPILER_ID}
        CMake current list directory:               ${CMAKE_CURRENT_LIST_DIR}
        CMake current source directory:             ${CMAKE_SOURCE_DIR}
        CMake current binary directory:             ${CMAKE_BINARY_DIR}
        CMake install directory prefix:             ${CMAKE_INSTALL_PREFIX}")

# =================================================================================
# Build configuration/settings
# =================================================================================

# By default, build SVF and its targets treating all compiler warnings as errors (except deprecations)
add_compile_options("$<$<BOOL:${SVF_WARN_AS_ERROR}>:-Wall>" "$<$<BOOL:${SVF_WARN_AS_ERROR}>:-Werror>"
                    "$<$<BOOL:${SVF_WARN_AS_ERROR}>:-Wno-deprecated-declarations>")

# Keep assertions enabled if requested
add_compile_options("$<$<BOOL:${SVF_ENABLE_ASSERTIONS}>:-UNDEBUG>")

# Export dynamic symbols if requested (adds "-export-dynamic" to linkers that support it to enable backtraces)
add_link_options("$<$<BOOL:${SVF_EXPORT_DYNAMIC}>:-rdynamic>")
add_link_options("$<$<BOOL:${SVF_EXPORT_DYNAMIC}>:-Wl,--export-dynamic>")

# Configure whether a coverage build should be created for SVF (i.e. add runtime instrumentation)
add_compile_options("$<$<OR:$<BOOL:${SVF_COVERAGE}>,$<BOOL:$ENV{SVF_COVERAGE}>>:-fprofile-arcs>"
                    "$<$<OR:$<BOOL:${SVF_COVERAGE}>,$<BOOL:$ENV{SVF_COVERAGE}>>:-ftest-coverage>")
add_link_options("$<$<OR:$<BOOL:${SVF_COVERAGE}>,$<BOOL:$ENV{SVF_COVERAGE}>>:-fprofile-arcs>"
                 "$<$<OR:$<BOOL:${SVF_COVERAGE}>,$<BOOL:$ENV{SVF_COVERAGE}>>:-ftest-coverage>")

# If building with sanitiser, load the given sanitiser mode
if(SVF_SANITIZE STREQUAL "address")
    add_compile_options("-fno-omit-frame-pointer" "-fsanitize=address")
    add_link_options("-fsanitize=address")
    message(STATUS "Sanitizer build: ${SVF_SANITIZE}")
elseif(SVF_SANITIZE STREQUAL "thread")
    add_compile_options("-fsanitize=thread")
    add_link_options("-fsanitize=thread")
    message(STATUS "Sanitizer build: ${SVF_SANITIZE}")
elseif(NOT SVF_SANITIZE STREQUAL "")
    message(FATAL_ERROR "Unknown sanitizer type: ${SVF_SANITIZE}")
endif()

# Enable debug information so that error reports are much more verbose (when using SVF)
add_compile_options("-g3")
add_link_options("-g3")

# =================================================================================
# Find & load external Z3 dependencies
# =================================================================================

# Find Z3 and its include directory from the top-level include file; try from CMake package first
find_package(
    Z3
    CONFIG
    HINTS
    ${Z3_DIR}
    $ENV{Z3_DIR})
if(Z3_FOUND)
    message(STATUS "Found Z3 CMake package:
        Z3 Version Major:               ${Z3_VERSION_MAJOR}
        Z3 Version Minor:               ${Z3_VERSION_MINOR}
        Z3 Version Patch:               ${Z3_VERSION_PATCH}
        Z3 Version Tweak:               ${Z3_VERSION_TWEAK}
        Z3 Version String:              ${Z3_VERSION_STRING}
        Z3 C Include Directories:       ${Z3_C_INCLUDE_DIRS}
        Z3 C++ Include Directories:     ${Z3_CXX_INCLUDE_DIRS}
        Z3 Libraries:                   ${Z3_LIBRARIES}")
else()
    find_library(
        Z3_LIBRARIES REQUIRED
        NAMES z3
        HINTS ${Z3_DIR} ENV Z3_DIR
        PATH_SUFFIXES bin lib)
    find_path(
        Z3_INCLUDES REQUIRED
        NAMES z3++.h
        HINTS ${Z3_DIR} ENV Z3_DIR
        PATH_SUFFIXES include z3)
    message(STATUS "Z3 STATUS:
        Z3 library file:        ${Z3_LIBRARIES}
        Z3 include directory:   ${Z3_INCLUDES}")

    # Add the Z3 include directory and link the Z3 library to all targets of SVF
    set(CMAKE_INSTALL_RPATH ${Z3_INCLUDES})
    include_directories(SYSTEM ${Z3_INCLUDES})
endif()

# =================================================================================
# Export C/C++ config.h header file
# =================================================================================

# Create config.h based on config.in; export to <build_dir>/include/Util/config.h
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/include)
configure_file(${PROJECT_SOURCE_DIR}/cmake/ConfigHdr.cmake.in ${PROJECT_BINARY_DIR}/include/Util/config.h)

# =================================================================================
# Test suite configuration & build
# =================================================================================

# Check if the test-suite is present, if it is then build bc files and add testing to cmake build
if(EXISTS ${PROJECT_SOURCE_DIR}/Test-Suite)
    include_directories(${PROJECT_SOURCE_DIR}/Test-Suite)
    enable_testing()
    add_subdirectory(Test-Suite)
    include(CTest)
endif()

# =================================================================================
# Include core SVF source trees
# =================================================================================

# Add the actual SVF and SVF-LLVM targets
add_subdirectory(svf)
add_subdirectory(svf-llvm)

# Whether RTTI/Exceptions are enabled currently depends on whether the LLVM instance used to build SVF had them enabled;
# since the LLVM instance is found in the "svf-llvm" subdirectory, it sets the below variables in its parent directory
# (i.e. for this CMakeLists.txt) so check them here
add_compile_definitions("$<IF:$<BOOL:${SVF_ENABLE_RTTI}>,,-fno-rtti>"
                        "$<IF:$<BOOL:${SVF_ENABLE_EXCEPTIONS}>,,-fno-exceptions>")

# =================================================================================
# Installation & package configuration
# =================================================================================

# Export the targets so that SvfCore & SvfLLVM are available after find_package(SVF) also in the build tree
export(
    EXPORT SVF_EXPORTED_TARGETS
    NAMESPACE SVF::
    FILE ${PROJECT_BINARY_DIR}/SVFTargets.cmake)

# Generate SVFConfigVersion.cmake
write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/SVFConfigVersion.cmake
    VERSION "${SVF_VERSION_MAJOR}.${SVF_VERSION_MINOR}"
    COMPATIBILITY AnyNewerVersion)

# Generate the package config
configure_file(${PROJECT_SOURCE_DIR}/cmake/SVF.pc.cmake.in ${PROJECT_BINARY_DIR}/SVF.pc @ONLY)

# Install the SVFTargets.cmake file along with the package
install(
    EXPORT SVF_EXPORTED_TARGETS
    FILE SVFTargets.cmake
    NAMESPACE SVF::
    DESTINATION ${SVF_INSTALL_CMAKEDIR})

# Also generate a relocatable package in the install tree
configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/SVFConfig.cmake.in ${PROJECT_BINARY_DIR}/SVFConfig.cmake
    INSTALL_DESTINATION ${SVF_INSTALL_CMAKEDIR}
    PATH_VARS SVF_INSTALL_BINDIR
              SVF_INSTALL_LIBDIR
              SVF_INSTALL_CMAKEDIR
              SVF_INSTALL_INCLUDEDIR
              SVF_EXTAPI_INSTALL_DIR
              SVF_EXTAPI_INSTALL_FILE)

# Actually install the configuration, versioning, and pkg_config files
install(FILES ${PROJECT_BINARY_DIR}/SVFConfigVersion.cmake DESTINATION ${SVF_INSTALL_CMAKEDIR})
install(FILES ${PROJECT_BINARY_DIR}/SVFConfig.cmake DESTINATION ${SVF_INSTALL_CMAKEDIR})
install(FILES ${PROJECT_BINARY_DIR}/SVF.pc DESTINATION ${SVF_INSTALL_PKGDIR})

# Install generated configuration header (see `configure_file()`) to top-level include dir of SVF
install(FILES ${PROJECT_BINARY_DIR}/include/Util/config.h DESTINATION ${SVF_INSTALL_INCLUDEDIR}/Util)

# Package with CPack
include(InstallRequiredSystemLibraries)
include(CPack)

# Since SVF builds into non-standard directories; symlink the compile commands
file(
    CREATE_LINK
    ${PROJECT_BINARY_DIR}/compile_commands.json
    ${PROJECT_SOURCE_DIR}/compile_commands.json
    COPY_ON_ERROR
    SYMBOLIC)
