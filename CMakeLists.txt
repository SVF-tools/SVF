cmake_minimum_required(VERSION 3.23)

# =================================================================================
# SVF project definition
# =================================================================================

project(
    SVF
    VERSION 3.1
    DESCRIPTION "SVF is a static value-flow analysis tool for LLVM-based languages"
    HOMEPAGE_URL "https://github.com/SVF-tools/SVF"
    LANGUAGES C CXX
    )

# Export compile commands for clangd & IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# If SVF is included as a subdirectory (add_subdirectory(SVF)), expose version info to parent scope
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(SVF_VERSION
        "${PROJECT_VERSION}"
        PARENT_SCOPE
        )
endif()

# Export version info to parent
set(SVF_VERSION
    ${PROJECT_VERSION}
    CACHE STRING "SVF version" FORCE
    )

# Ensure installation directories like ${CMAKE_INSTALL_LIBDIR} are available
include(GNUInstallDirs)

# Include helpers to package the SVF CMake package
include(CMakePackageConfigHelpers)

# Allow checking for IPO support by the used compiler
include(CheckIPOSupported)

# Since SVF builds into non-standard directories; symlink the compile commands into the top-level directory
file(CREATE_LINK ${SVF_BINARY_DIR}/compile_commands.json compile_commands.json COPY_ON_ERROR SYMBOLIC)

# Ensure the configuration input files are findable by default
list(APPEND CMAKE_MODULE_PATH "${SVF_SOURCE_DIR}/cmake")

# Fetch some global configuration options
set(SVF_BUILD_TYPE ${CMAKE_BUILD_TYPE})
set(SVF_SHARED_LIBS ${BUILD_SHARED_LIBS})
set(SVF_CXX_STANDARD ${CMAKE_CXX_STANDARD})

# =================================================================================
# SVF options & settings
# =================================================================================

# Configurable (string) options for building SVF
set(SVF_SANITIZE
    ""
    CACHE STRING "Create sanitizer build (address)"
    )

# Sanity check
if(SVF_SANITIZE
   AND NOT (SVF_SANITIZE STREQUAL "thread")
   AND NOT (SVF_SANITIZE STREQUAL "address")
    )
    message(FATAL_ERROR "Unknown sanitizer type: ${SVF_SANITIZE}")
endif()

# Configurable (boolean) options for building SVF
option(SVF_USE_PIC "Enable PIC" ON)
option(SVF_USE_LTO "Enable full LTO")
option(SVF_USE_LLD "Use the ld.lld linker")
option(SVF_COVERAGE "Create coverage build")
option(SVF_DEBUG_INFO "Explicitly preserve debug info (also in release builds)")
option(SVF_WARN_AS_ERROR "Treat all compiler warnings as errors when building SVF (default: on)" ON)
option(SVF_EXPORT_DYNAMIC "Export all (not only the actually used) dynamic symbols to dynamic symbol table")
option(SVF_ENABLE_ASSERTIONS "Always enable debugging assertions, also if the build type is a release build")

# If building dynamic libraries, always enable PIC
if(SVF_SHARED_LIBS)
    if(NOT SVF_USE_PIC)
        message(FATAL_ERROR "Cannot disable PIC when compiling dynamic shared libraries!")
    endif()
    set(SVF_USE_PIC ON)
endif()

# Ensure the compiler supports LTO if building with LTO enabled
if(SVF_USE_LTO)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(NOT ipo_supported)
        message(FATAL_ERROR "Cannot build with LTO; compiler doesn't support LTO")
    endif()
endif()

# Configure top-level SVF variables (used by CMake for configuring installed SVF package)
set(SVF_INSTALL_BINDIR
    ${CMAKE_INSTALL_BINDIR}
    CACHE PATH "Set the installation location for executables (tools)"
    )
set(SVF_INSTALL_LIBDIR
    ${CMAKE_INSTALL_LIBDIR}
    CACHE PATH "Set the installation location for library files (libSvfCore/libSvfLLVM)"
    )
set(SVF_INSTALL_EXTAPIDIR
    ${CMAKE_INSTALL_INCLUDEDIR}/SVF
    CACHE PATH "Set the installation location for the extapi.bc file; defaults to \"include/SVF/...\""
    )
set(SVF_INSTALL_INCLUDEDIR
    ${CMAKE_INSTALL_INCLUDEDIR}
    CACHE PATH "Set the installation location for includes; defaults to \"include/...\""
    )
set(SVF_INSTALL_PKGCONFDIR
    ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    CACHE PATH "Set the installation location for pkgconf files; default: \"lib/pkgconfig/...\""
    )
set(SVF_INSTALL_CMAKECONFIGDIR
    ${CMAKE_INSTALL_LIBDIR}/cmake/SVF
    CACHE PATH "Set the installation location for CMake package files; default: \"lib/cmake/SVF/...\""
    )

# Set location of extapi.bc (installed)
set(SVF_EXTAPI_BC_NAME extapi.bc)
set(SVF_BUILD_EXTAPI_BC ${SVF_BINARY_DIR}/lib/${SVF_EXTAPI_BC_NAME})
set(SVF_INSTALL_EXTAPI_BC ${SVF_INSTALL_EXTAPIDIR}/${SVF_EXTAPI_BC_NAME})

message(
    STATUS
        "Building ${PROJECT_NAME} with configuration:
  SVF version:                                  ${SVF_VERSION}
  SVF root directory:                           ${SVF_SOURCE_DIR}
  SVF binary directory:                         ${SVF_BINARY_DIR}
  SVF option - create sanitiser build (str):    ${SVF_SANITIZE}
  SVF option - coverage build:                  ${SVF_COVERAGE}
  SVF option - warnings as errors:              ${SVF_WARN_AS_ERROR}
  SVF option - unused dynamic symbols:          ${SVF_EXPORT_DYNAMIC}
  SVF option - enable build assertions:         ${SVF_ENABLE_ASSERTIONS}"
    )

message(
    STATUS
        "Using CMake build configuration:
  CMake generator:                              ${CMAKE_GENERATOR}
  CMake C compiler:                             ${CMAKE_C_COMPILER_ID}
  CMake C++ compiler:                           ${CMAKE_CXX_COMPILER_ID}
  CMake current list directory:                 ${CMAKE_CURRENT_LIST_DIR}
  CMake current source directory:               ${CMAKE_CURRENT_SOURCE_DIR}
  CMake current binary directory:               ${CMAKE_CURRENT_BINARY_DIR}"
    )

# =================================================================================
# SVF Z3 dependency finding
# =================================================================================

# Try finding a system-wide (or with $Z3_DIR) Z3's CMake package to import directly
find_package(Z3 CONFIG HINTS ${Z3_DIR} $ENV{Z3_DIR})

# Use system-wide Z3 package if found; otherwise search locally
if(Z3_FOUND)
    set(z3_lib z3::libz3)
    message(STATUS "Found Z3 CMake package (version: ${Z3_VERSION_STRING})")
else()
    message(STATUS "Failed to find Z3 CMake package; searching for library & header files")

    # Find the library files & header; these must succeed
    find_library(
        Z3_LIB_DIRS REQUIRED
        NAMES z3 libz3.a libz3.so
        HINTS ${Z3_DIR} ENV Z3_DIR
        )
    find_path(
        Z3_INCLUDE_DIRS REQUIRED
        NAMES z3++.h
        HINTS ${Z3_DIR} ENV Z3_DIR
        )

    # Manually create an imported target to link against with target_link_libraries
    message(STATUS "Found Z3 libraries and headers: ${Z3_LIB_DIRS} & ${Z3_INCLUDE_DIRS}")
    add_library(z3::libz3 UNKNOWN IMPORTED)
    set_target_properties(z3::libz3 PROPERTIES IMPORTED_LOCATION "${Z3_LIB_DIRS}")
    set_target_properties(z3::libz3 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${Z3_INCLUDE_DIRS}")
    set(z3_lib z3::libz3)
endif()

# =================================================================================
# SVF configuration interface library
# =================================================================================

# Define an interface library which contains publically exposed properties/flags/defs
add_library(SvfFlags INTERFACE)
add_library(SVF::Flags ALIAS SvfFlags)

# Set the C++ standard as a public required feature
target_compile_features(SvfFlags INTERFACE c_std_11 cxx_std_17)

# Expose install include tree publically (expose with SVF prefix for legacy support)
target_include_directories(
    SvfFlags
    INTERFACE $<INSTALL_INTERFACE:${SVF_INSTALL_INCLUDEDIR}>
    INTERFACE $<INSTALL_INTERFACE:${SVF_INSTALL_INCLUDEDIR}/SVF>
    )

# Expose build-tree include directories only within build interface (in-tree users)
target_include_directories(
    SvfFlags
    INTERFACE $<BUILD_INTERFACE:${SVF_BINARY_DIR}/include>
    INTERFACE $<BUILD_INTERFACE:${SVF_BINARY_DIR}/include/SVF>
    )

# Expose the location of extapi.bc during build & installation (use local during build)
target_compile_definitions(
    SvfFlags
    INTERFACE $<BUILD_INTERFACE:SVF_INSTALL_EXTAPI_BC="${SVF_BUILD_EXTAPI_BC}">
    INTERFACE $<INSTALL_INTERFACE:SVF_INSTALL_EXTAPI_BC="${CMAKE_INSTALL_PREFIX}/${SVF_INSTALL_EXTAPI_BC}">
    )

# Expose configured compiler & linker options through the interface (avoids repetition)
target_compile_options(
    SvfFlags
    INTERFACE $<BUILD_INTERFACE:$<$<BOOL:${SVF_DEBUG_INFO}>:-g3>>
    INTERFACE $<BUILD_INTERFACE:$<$<BOOL:${SVF_WARN_AS_ERROR}>:-Wall>>
    INTERFACE $<BUILD_INTERFACE:$<$<BOOL:${SVF_WARN_AS_ERROR}>:-Werror>>
    INTERFACE $<BUILD_INTERFACE:$<$<BOOL:${SVF_ENABLE_ASSERTIONS}>:-UNDEBUG>>
    INTERFACE $<BUILD_INTERFACE:$<$<STREQUAL:${SVF_SANITIZE},thread>:-fsanitize=thread>>
    INTERFACE $<BUILD_INTERFACE:$<$<STREQUAL:${SVF_SANITIZE},address>:-fsanitize=address>>
    INTERFACE $<BUILD_INTERFACE:$<$<BOOL:${SVF_WARN_AS_ERROR}>:-Wno-deprecated-declarations>>
    INTERFACE $<BUILD_INTERFACE:$<$<STREQUAL:${SVF_SANITIZE},address>:-fno-omit-frame-pointer>>
    INTERFACE $<BUILD_INTERFACE:$<$<OR:$<BOOL:${SVF_COVERAGE}>,$<BOOL:$ENV{SVF_COVERAGE}>>:-fprofile-arcs>>
    INTERFACE $<BUILD_INTERFACE:$<$<OR:$<BOOL:${SVF_COVERAGE}>,$<BOOL:$ENV{SVF_COVERAGE}>>:-ftest-coverage>>
    )
target_link_options(
    SvfFlags
    INTERFACE $<BUILD_INTERFACE:$<$<BOOL:${SVF_DEBUG_INFO}>:-g3>>
    INTERFACE $<BUILD_INTERFACE:$<$<BOOL:${SVF_USE_LLD}>:-fuse-ld=lld>>
    INTERFACE $<BUILD_INTERFACE:$<$<BOOL:${SVF_EXPORT_DYNAMIC}>:-rdynamic>>
    INTERFACE $<BUILD_INTERFACE:$<$<BOOL:${SVF_EXPORT_DYNAMIC}>:-Wl,--export-dynamic>>
    INTERFACE $<BUILD_INTERFACE:$<$<STREQUAL:${SVF_SANITIZE},thread>:-fsanitize=thread>>
    INTERFACE $<BUILD_INTERFACE:$<$<STREQUAL:${SVF_SANITIZE},address>:-fsanitize=address>>
    INTERFACE $<BUILD_INTERFACE:$<$<OR:$<BOOL:${SVF_COVERAGE}>,$<BOOL:$ENV{SVF_COVERAGE}>>:-fprofile-arcs>>
    INTERFACE $<BUILD_INTERFACE:$<$<OR:$<BOOL:${SVF_COVERAGE}>,$<BOOL:$ENV{SVF_COVERAGE}>>:-ftest-coverage>>
    )

# Add Z3 as a public dependency on the interface to ensure any users inherit the dependency
target_include_directories(SvfFlags SYSTEM INTERFACE ${Z3_INCLUDE_DIRS})
target_link_libraries(SvfFlags INTERFACE ${z3_lib})

# Ensure the interface library is exposed during installation
install(TARGETS SvfFlags EXPORT SVFTargets)

# =================================================================================
# SVF test suite
# =================================================================================

# If ./Test-Suite exists, add & run the tests
if(EXISTS "${SVF_SOURCE_DIR}/Test-Suite")
    include(CTest)
    enable_testing()
    add_subdirectory(Test-Suite)
endif()

# =================================================================================
# SVF core definitions
# =================================================================================

add_subdirectory(svf)
add_subdirectory(svf-llvm)

# =================================================================================
# SVF configuration header
# =================================================================================

# (1) Generate config.h into <build_tree>/include/SVF/Util; (2) Install it under <install_prefix>/include/SVF/Util
configure_file(${SVF_SOURCE_DIR}/cmake/SVFConfigHdr.cmake.in ${SVF_BINARY_DIR}/include/SVF/Util/config.h @ONLY)
install(FILES ${SVF_BINARY_DIR}/include/SVF/Util/config.h DESTINATION ${SVF_INSTALL_INCLUDEDIR}/SVF/Util)

# =================================================================================
# SVF CMake package configuration
# =================================================================================

# Export targets for in-tree `find_package(SVF)` support
export(
    EXPORT SVFTargets
    NAMESPACE SVF::
    FILE ${SVF_BINARY_DIR}/SVFTargets.cmake
    )

# Install the SVFTargets.cmake file along with the package
install(
    EXPORT SVFTargets
    NAMESPACE SVF::
    DESTINATION ${SVF_INSTALL_CMAKECONFIGDIR}
    )

# Create the CMake configuration file (to find SVF with find_package(SVF))
configure_package_config_file(
    cmake/SVFConfig.cmake.in ${SVF_BINARY_DIR}/SVFConfig.cmake
    INSTALL_DESTINATION ${SVF_INSTALL_CMAKECONFIGDIR}
    PATH_VARS SVF_INSTALL_BINDIR
              SVF_INSTALL_LIBDIR
              SVF_INSTALL_EXTAPIDIR
              SVF_INSTALL_EXTAPI_BC
              SVF_INSTALL_INCLUDEDIR
              SVF_INSTALL_PKGCONFDIR
              SVF_INSTALL_CMAKECONFIGDIR
    )

# Create the CMake version configuration package (to support finding specific SVF versions)
write_basic_package_version_file(
    ${SVF_BINARY_DIR}/SVFConfigVersion.cmake
    VERSION ${SVF_VERSION}
    COMPATIBILITY AnyNewerVersion
    )

# Install the generated configuration files
install(FILES ${SVF_BINARY_DIR}/SVFConfig.cmake DESTINATION ${SVF_INSTALL_CMAKECONFIGDIR})
install(FILES ${SVF_BINARY_DIR}/SVFConfigVersion.cmake DESTINATION ${SVF_INSTALL_CMAKECONFIGDIR})

# =================================================================================
# SVF pkgconf package configuration
# =================================================================================

configure_file(cmake/SVF.pc.in ${SVF_BINARY_DIR}/SVF.pc @ONLY)
install(FILES ${SVF_BINARY_DIR}/SVF.pc DESTINATION ${SVF_INSTALL_PKGCONFDIR})
