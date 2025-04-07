cmake_minimum_required(VERSION 3.23)

project(SVF
  VERSION 3.1
  DESCRIPTION "SVF is a static value-flow analysis tool for LLVM-based languages"
  HOMEPAGE_URL "https://github.com/SVF-tools/SVF"
  LANGUAGES C CXX
)
# Export version info to parent
set(SVF_VERSION "${PROJECT_VERSION}" PARENT_SCOPE)

# Ensure installation directories like ${CMAKE_INSTALL_LIBDIR} are available
include(GNUInstallDirs)

# Configurable (string) options for building SVF
set(SVF_SANITIZE "" CACHE STRING "Create sanitizer build (address)")

# Configurable (boolean) options for building SVF
option(SVF_COVERAGE "Create coverage build")
option(SVF_WARN_AS_ERROR "Treat warnings as errors when building SVF (default: on)" ON)
option(SVF_EXPORT_DYNAMIC "Export all (not only used) dynamic symbols to dynamic symbol table")
option(SVF_ENABLE_ASSERTIONS "Always enable assertions")

# Configure top-level SVF variables (used by CMake for configuring installed SVF package)
set(SVF_INSTALL_ROOT ${CMAKE_INSTALL_PREFIX})
set(SVF_INSTALL_BIN_DIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR})
set(SVF_INSTALL_LIB_DIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
set(SVF_INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR})

# Set where extapi.bc is exported to in the installed CMake package
set(SVF_INSTALL_EXTAPI_DIR ${SVF_INSTALL_LIB_DIR})
set(SVF_INSTALL_EXTAPI_FILE ${SVF_INSTALL_EXTAPI_DIR}/extapi.bc)

message(STATUS "Building ${PROJECT_NAME} with configuration:
  SVF version:                                  ${SVF_VERSION}
  SVF root directory:                           ${SVF_SOURCE_DIR}
  SVF binary directory:                         ${SVF_BINARY_DIR}
  SVF option - create sanitiser build (str):    ${SVF_SANITIZE}
  SVF option - coverage build:                  ${SVF_COVERAGE}
  SVF option - warnings as errors:              ${SVF_WARN_AS_ERROR}
  SVF option - unused dynamic symbols:          ${SVF_EXPORT_DYNAMIC}
  SVF option - enable build assertions:         ${SVF_ENABLE_ASSERTIONS}")

message(STATUS "Using CMake build configuration:
  CMake generator:                              ${CMAKE_GENERATOR}
  CMake C compiler:                             ${CMAKE_C_COMPILER_ID}
  CMake C++ compiler:                           ${CMAKE_CXX_COMPILER_ID}
  CMake current list directory:                 ${CMAKE_CURRENT_LIST_DIR}
  CMake current source directory:               ${CMAKE_CURRENT_SOURCE_DIR}
  CMake current binary directory:               ${CMAKE_CURRENT_BINARY_DIR}")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include)
# Create config.h based on config.in
configure_file(${SVF_SOURCE_DIR}/.config.in ${SVF_BINARY_DIR}/include/Util/config.h)

# Include the directory where the configuration header is exported for all targets
include_directories(${SVF_BINARY_DIR}/include)

# Install generated configuration header (see `configure_file()`) to top-level include dir of SVF
install(
  FILES ${SVF_BINARY_DIR}/include/Util/config.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Util
)

# Build SVF with C++ standard C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)

# By default, build SVF and its targets treating all compiler warnings as errors (except deprecations)
add_compile_options(
  "$<$<BOOL:${SVF_WARN_AS_ERROR}>:-Wall>"
  "$<$<BOOL:${SVF_WARN_AS_ERROR}>:-Werror>"
  "$<$<BOOL:${SVF_WARN_AS_ERROR}>:-Wno-deprecated-declarations>"
)

# Keep assertions enabled if requested
add_compile_options("$<$<BOOL:${SVF_ENABLE_ASSERTIONS}>:-UNDEBUG>")

# Export dynamic symbols if requested (adds "-export-dynamic" to linkers that support it to enable backtraces)
add_compile_options("$<$<BOOL:${SVF_EXPORT_DYNAMIC}>:-rdynamic>")

# Configure whether a coverage build should be created for SVF (i.e. add runtime instrumentation)
add_compile_options(
  "$<$<OR:$<BOOL:${SVF_COVERAGE}>,$<BOOL:$ENV{SVF_COVERAGE}>>:-fprofile-arcs>"
  "$<$<OR:$<BOOL:${SVF_COVERAGE}>,$<BOOL:$ENV{SVF_COVERAGE}>>:-ftest-coverage>"
)
add_link_options(
  "$<$<OR:$<BOOL:${SVF_COVERAGE}>,$<BOOL:$ENV{SVF_COVERAGE}>>:-fprofile-arcs>"
  "$<$<OR:$<BOOL:${SVF_COVERAGE}>,$<BOOL:$ENV{SVF_COVERAGE}>>:-ftest-coverage>"
)

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

# Check if the test-suite is present, if it is then build bc files and add testing to cmake build
if(EXISTS "${PROJECT_SOURCE_DIR}/Test-Suite")
  include_directories(${PROJECT_SOURCE_DIR}/Test-Suite)
  enable_testing()
  add_subdirectory(Test-Suite)
  include(CTest)
endif()

# Find Z3 and its include directory from the top-level include file
find_library(
  Z3_LIBRARIES
  REQUIRED
  NAMES z3
  HINTS ${Z3_DIR} ENV Z3_DIR
  PATH_SUFFIXES bin lib)
find_path(
  Z3_INCLUDES
  REQUIRED
  NAMES z3++.h
  HINTS ${Z3_DIR} ENV Z3_DIR
  PATH_SUFFIXES include z3)
message(STATUS "Z3 STATUS:
  Z3 library file:        ${Z3_LIBRARIES}
  Z3 include directory:   ${Z3_INCLUDES}")

# Add the Z3 include directory and link the Z3 library to all targets of SVF
set(CMAKE_INSTALL_RPATH ${Z3_INCLUDES})
link_libraries(${Z3_LIBRARIES})
include_directories(SYSTEM ${Z3_INCLUDES})

# Add the actual SVF and SVF-LLVM targets
add_subdirectory(svf)
add_subdirectory(svf-llvm)

# Whether RTTI/Exceptions are enabled currently depends on whether the LLVM instance used to build
# SVF had them enabled; since the LLVM instance is found in the "svf-llvm" subdirectory, it sets the
# below variables in its parent directory (i.e. for this CMakeLists.txt) so check them here
add_compile_definitions(
  $<IF:$<BOOL:${SVF_ENABLE_RTTI}>,,-fno-rtti>
  $<IF:$<BOOL:${SVF_ENABLE_EXCEPTIONS}>,,-fno-exceptions>
)

# Export the SVF target for including SVF from CMake using find_package(SVF)
include(CMakePackageConfigHelpers)

# Generate SVFConfig.cmake
configure_package_config_file(
  .config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/lib/cmake/SVF/SVFConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SVF
  PATH_VARS
    SVF_INSTALL_ROOT
    SVF_INSTALL_BIN_DIR
    SVF_INSTALL_LIB_DIR
    SVF_INSTALL_INCLUDE_DIR
    SVF_INSTALL_EXTAPI_DIR
    SVF_INSTALL_EXTAPI_FILE
)

# Generate SVFConfigVersion.cmake
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/lib/cmake/SVF/SVFConfigVersion.cmake"
  VERSION "${SVF_VERSION_MAJOR}.${SVF_VERSION_MINOR}"
  COMPATIBILITY AnyNewerVersion
)

# Install above CMake files as part of installation
install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/lib/cmake/SVF/SVFConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/lib/cmake/SVF/SVFConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SVF
)
