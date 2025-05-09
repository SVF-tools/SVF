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

# Set SVF's default C/C++ standards (C11/C++17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure all build artifacts end up in <build>/bin, <build>/lib, etc.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${SVF_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SVF_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${SVF_BINARY_DIR}/lib)

# If SVF is included as a subdirectory (add_subdirectory(SVF)), expose version info to parent scope
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(SVF_VERSION
      "${PROJECT_VERSION}"
      PARENT_SCOPE
  )
endif()

# Ensure installation directories like ${CMAKE_INSTALL_LIBDIR} are available
include(GNUInstallDirs)

# Include helpers to package the SVF CMake package
include(CMakePackageConfigHelpers)

# Allow checking for IPO support by the used compiler
include(CheckIPOSupported)

# Since SVF builds into non-standard directories; symlink/copy compile commands into top-level directory
if(WIN32
   OR MINGW
   OR MSYS
   OR CYGWIN
)
  file(
    COPY ${SVF_BINARY_DIR}/compile_commands.json
    DESTINATION ${CMAKE_CURRENT_LIST_DIR}
    RENAME compile_commands.json
  )
else()
  file(CREATE_LINK ${SVF_BINARY_DIR}/compile_commands.json compile_commands.json COPY_ON_ERROR SYMBOLIC)
endif()

# Ensure the configuration input files & module definitions are findable by default
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake" "${CMAKE_CURRENT_LIST_DIR}/cmake/Modules")

# Store global build opts
set(SVF_BUILD_TYPE ${CMAKE_BUILD_TYPE})
set(SVF_SHARED_LIBS ${BUILD_SHARED_LIBS})

# =================================================================================
# SVF options & settings
# =================================================================================

# Configurable (string) options for building SVF
set(SVF_SANITIZE
    ""
    CACHE STRING "Create sanitizer build (address)"
)

# Sanity check
if(SVF_SANITIZE AND NOT (SVF_SANITIZE STREQUAL "thread" OR SVF_SANITIZE STREQUAL "address"))
  message(FATAL_ERROR "Unrecognised sanitiser type: ${SVF_SANITIZE}")
endif()

# Configurable (boolean) options for building SVF
option(SVF_NO_EH "Disable exception handling (-fno-exceptions)")
option(SVF_NO_RTTI "Disable runtime type information (-fno-rtti)")
option(SVF_USE_PIC "Compile with position-independent code (-fPIC)" ON)
option(SVF_USE_LTO "Compile with link-time optimisations enabled (-flto)")
option(SVF_USE_LLD "Use LLVM's ld.lld linker instead of the default linker")
option(SVF_COVERAGE "Create a coverage build (-fprofile-arcs & -ftest-coverage)")
option(SVF_DEBUG_INFO "Enable & explicitly preserve debug info (-g3); also in release builds")
option(SVF_WARN_AS_ERROR "Treat all compiler warnings as errors when building SVF (default: on)" ON)
option(SVF_EXPORT_DYNAMIC "Export all (not only the actually used) symbols to dynamic symbol table" OFF)
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
    CACHE PATH "Set binaries install dir"
)
set(SVF_INSTALL_LIBDIR
    ${CMAKE_INSTALL_LIBDIR}
    CACHE PATH "Set libraries install dir"
)
set(SVF_INSTALL_EXTAPIDIR
    ${CMAKE_INSTALL_INCLUDEDIR}/SVF
    CACHE PATH "Set extapi.bc install dir"
)
set(SVF_INSTALL_INCLUDEDIR
    ${CMAKE_INSTALL_INCLUDEDIR}
    CACHE PATH "Set public headers install dir"
)
set(SVF_INSTALL_PKGCONFDIR
    ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    CACHE PATH "Override pkgconfig install dir"
)
set(SVF_INSTALL_CMAKECONFIGDIR
    ${CMAKE_INSTALL_LIBDIR}/cmake/SVF
    CACHE PATH "Set CMake package install dir"
)

# Set location of extapi.bc (installed)
set(SVF_EXTAPI_BC_NAME extapi.bc)
set(SVF_BUILD_EXTAPI_BC ${SVF_BINARY_DIR}/lib/${SVF_EXTAPI_BC_NAME})
set(SVF_INSTALL_EXTAPI_BC ${SVF_INSTALL_EXTAPIDIR}/${SVF_EXTAPI_BC_NAME})

# Depending on the configuration, globally enable PIC/LTO/LLD for targets defined hereafter
set(CMAKE_POSITION_INDEPENDENT_CODE
    ${SVF_USE_PIC}
    CACHE BOOL "" FORCE
)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION
    ${SVF_USE_LTO}
    CACHE BOOL "" FORCE
)

# Setting the linker type like this is only supported since CMake 3.29+
if(SVF_USE_LLD)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.29")
    set(CMAKE_LINKER_TYPE
        LLD
        CACHE STRING "" FORCE
    )
  endif()
endif()

message(
  STATUS
    "Using SVF build configuration:
  Project:                                          ${PROJECT_NAME}
  SVF version:                                      ${SVF_VERSION}
  SVF root directory:                               ${SVF_SOURCE_DIR}
  SVF binary directory:                             ${SVF_BINARY_DIR}
  Project root directory:                           ${PROJECT_SOURCE_DIR}
  Project binary directory:                         ${PROJECT_BINARY_DIR}

  Installing executables to:                        ${SVF_INSTALL_BINDIR}
  Installing libraries to:                          ${SVF_INSTALL_LIBDIR}
  Installing headers to:                            ${SVF_INSTALL_INCLUDEDIR}
  Installing extapi.bc to:                          ${SVF_INSTALL_EXTAPIDIR}
  Installing pkgconfig to:                          ${SVF_INSTALL_PKGCONFDIR}
  Installing CMake package to:                      ${SVF_INSTALL_CMAKECONFIGDIR}

  SVF option - Build type:                          ${SVF_BUILD_TYPE}
  SVF option - Building shared libs:                ${SVF_SHARED_LIBS}
  SVF option - Disabling exception handling:        ${SVF_NO_EH}
  SVF option - Disabling runtime type information:  ${SVF_NO_RTTI}
  SVF option - Enabling position-independent code:  ${SVF_USE_PIC}
  SVF option - Enabling link-time optimisations:    ${SVF_USE_LTO}
  SVF option - Using ld.lld as default linker:      ${SVF_USE_LLD}
  SVF option - Enabling coverage build:             ${SVF_COVERAGE}
  SVF option - Treating warnings as errors:         ${SVF_DEBUG_INFO}
  SVF option - Generating debug information:        ${SVF_WARN_AS_ERROR}
  SVF option - Exporting all dynamic symbols:       ${SVF_EXPORT_DYNAMIC}
  SVF option - Forcefully enabling assertions:      ${SVF_ENABLE_ASSERTIONS}

  CMake root directory:                             ${CMAKE_SOURCE_DIR}
  CMake binary directory:                           ${CMAKE_BINARY_DIR}
  Current CMake root directory:                     ${CMAKE_CURRENT_SOURCE_DIR}
  Current CMake binary directory:                   ${CMAKE_CURRENT_BINARY_DIR}

  CMake generator:                                  ${CMAKE_GENERATOR}
  CMake C compiler:                                 ${CMAKE_C_COMPILER_ID}
  CMake C++ compiler:                               ${CMAKE_CXX_COMPILER_ID}
  Using CMake C standard:                           ${CMAKE_C_STANDARD}
  Using CMake C++ standard:                         ${CMAKE_CXX_STANDARD}
  Current CMake install prefix:                     ${CMAKE_INSTALL_PREFIX}"
)

# =================================================================================
# SVF Z3 dependency finding
# =================================================================================

# Find the local FindZ3.cmake package; internally finds system Z3; falls back to manual search
find_package(Z3 REQUIRED)
message(
  STATUS
    "Using Z3 package:
  Z3 major version:     ${Z3_VERSION_MAJOR}
  Z3 minor version:     ${Z3_VERSION_MINOR}
  Z3 patch version:     ${Z3_VERSION_PATCH}
  Z3 tweak version:     ${Z3_VERSION_TWEAK}
  Z3 version string:    ${Z3_VERSION_STRING}
  Z3 link libraries:    ${Z3_LIBRARIES}
  Z3 C include dirs:    ${Z3_C_INCLUDE_DIRS}
  Z3 C++ include dirs:  ${Z3_CXX_INCLUDE_DIRS}"
)

# =================================================================================
# SVF configuration interface library
# =================================================================================

# Define an interface library which contains publically exposed properties/flags/defs
add_library(SvfFlags INTERFACE)
add_library(SVF::Flags ALIAS SvfFlags)

# Set the C++ standard as a public required feature
target_compile_features(SvfFlags INTERFACE c_std_11 cxx_std_17)

# Expose headers in build-tree only to in-tree users (not as system headers; in-tree users see warnings)
target_include_directories(
  SvfFlags
  INTERFACE $<BUILD_INTERFACE:${SVF_BINARY_DIR}>
  INTERFACE $<BUILD_INTERFACE:${SVF_BINARY_DIR}/include>
  INTERFACE $<BUILD_INTERFACE:${SVF_BINARY_DIR}/include/SVF>
)

# Expose installed headers publicly (as system headers to suppress internal warnings to end-users)
target_include_directories(
  SvfFlags
  SYSTEM
  INTERFACE $<INSTALL_INTERFACE:${SVF_INSTALL_INCLUDEDIR}>
  INTERFACE $<INSTALL_INTERFACE:${SVF_INSTALL_INCLUDEDIR}/SVF>
)

# Expose build-tree compiled artifacts only to in-tree users; expose install tree publicly
target_link_directories(
  SvfFlags
  INTERFACE $<BUILD_INTERFACE:${SVF_BINARY_DIR}>
  INTERFACE $<BUILD_INTERFACE:${SVF_BINARY_DIR}/lib>
  INTERFACE $<INSTALL_INTERFACE:${SVF_INSTALL_LIBDIR}>
)

# Expose build-tree extapi.bc to in-tree users of SVF (i.e. building SVF as subproject)
target_compile_definitions(SvfFlags INTERFACE $<BUILD_INTERFACE:SVF_INSTALL_EXTAPI_BC="${SVF_BUILD_EXTAPI_BC}">)

# Expose required ABI flags publicly; expose other build flags only to in-tree users of SVF
target_compile_options(
  SvfFlags
  INTERFACE $<$<BOOL:${SVF_NO_RTTI}>:-fno-rtti>
  INTERFACE $<$<BOOL:${SVF_NO_EH}>:-fno-exceptions>
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
  INTERFACE $<$<BOOL:${SVF_NO_RTTI}>:-fno-rtti>
  INTERFACE $<$<BOOL:${SVF_NO_EH}>:-fno-exceptions>
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
target_link_libraries(SvfFlags INTERFACE ${Z3_LIBRARIES})

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
# SVF pkgconf package configuration
# =================================================================================

configure_file(cmake/SVF.pc.in ${SVF_BINARY_DIR}/SVF.pc @ONLY)
install(FILES ${SVF_BINARY_DIR}/SVF.pc DESTINATION ${SVF_INSTALL_PKGCONFDIR})

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

# Also install the extra module finding package definitions (e.g., FindZ3.cmake)
install(
  DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/cmake/Modules"
  DESTINATION "${SVF_INSTALL_CMAKECONFIGDIR}"
  FILES_MATCHING
  PATTERN "*.cmake"
)
