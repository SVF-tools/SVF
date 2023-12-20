cmake_minimum_required(VERSION 3.13.4)

project("SVF")

configure_file(${PROJECT_SOURCE_DIR}/.config.in
               ${PROJECT_BINARY_DIR}/include/Util/config.h)

# Match configuration used to build LLVM (match C++ standard; match
# runtime typing information (RTTI); match exception handling; etc)
if(COMMAND add_llvm_library)
  message(STATUS "Detected in-tree build configuration; skipping LLVM fetching")
  set(IN_SOURCE_BUILD 1)
else()
  message(STATUS "Detected out-of-tree build configuration; fetching LLVM")

  find_package(LLVM REQUIRED CONFIG HINTS ${LLVM_DIR} $ENV{LLVM_DIR})
  message(STATUS "LLVM STATUS:
    Version       ${LLVM_VERSION}
    Definitions   ${LLVM_DEFINITIONS}
    Includes      ${LLVM_INCLUDE_DIRS}
    Libraries     ${LLVM_LIBRARY_DIRS}
    Targets       ${LLVM_TARGETS_TO_BUILD}
    Build type    ${LLVM_BUILD_TYPE}
    Exceptions    ${LLVM_ENABLE_EH}
    RTTI          ${LLVM_ENABLE_RTTI}
    Dynamic lib   ${LLVM_LINK_LLVM_DYLIB}"
  )

  if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT ${LLVM_BUILD_TYPE} STREQUAL "Debug")
    message(NOTICE "Building SVF in debug-mode but LLVM was not built in debug-mode; "
                   "debug information could be incomplete when using SVF from LLVM")
  endif()

  if(NOT LLVM_ENABLE_EH)
    message(STATUS "Building SVF without exception handling")
    add_compile_options("-fno-exceptions")
  endif()

  if(NOT LLVM_ENABLE_RTTI)
    message(STATUS "Building SVF without RTII")
    add_compile_options("-fno-rtti")
  endif()

  # Load the LLVM definitions & include directories
  separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

  if(NOT ${LLVM_LINK_LLVM_DYLIB})
    message(STATUS "Linking to separate LLVM static libraries")
    llvm_map_components_to_libnames(
      llvm_libs
      analysis
      bitwriter
      core
      instcombine
      instrumentation
      ipo
      irreader
      linker
      scalaropts
      support
      target
      transformutils)
  else()
    message(STATUS "Linking to LLVM dynamic shared library object")
    set(llvm_libs LLVM)
  endif()

  # Make the "add_llvm_library()" command available
  include(AddLLVM)

  # Set the default C++ standard used to build SVF; LLVM versions up to version 6 defaulted to C++98,
  # and up to (and including) version 15 the default was C++14. From versions 16 and onwards, the
  # default C++ standard has been C++17. Build SVF with C++14 unless the LLVM version is below 6.
  if (LLVM_VERSION_MAJOR VERSION_LESS 6)
    set(CMAKE_CXX_STANDARD 98)
    message(STATUS "Got LLVM version ${LLVM_VERSION}; using C++ standard: ${CMAKE_CXX_STANDARD}")
  elseif(LLVM_VERSION_MAJOR VERSION_LESS 16)
    set(CMAKE_CXX_STANDARD 14)
    message(STATUS "Got LLVM version ${LLVM_VERSION}; using C++ standard: ${CMAKE_CXX_STANDARD}")
  else()
    message(FATAL_ERROR "Found unsupported LLVM version; need <= 15, got: ${LLVM_VERSION}")
  endif()

  # Always enable extensions and require a compiler that supports at least the set C++ standard
  set(CMAKE_CXX_EXTENSIONS ON)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)

endif()

# Treat compiler warnings as errors
add_compile_options("-Werror" "-Wall")

# Keep assertions enabled if requested
option(SVF_ENABLE_ASSERTIONS "Always enable assertions")
if(SVF_ENABLE_ASSERTIONS)
  add_compile_options("-UNDEBUG")
endif()

# Turn this on if you need symbols (e.g., use them for backtrace debugging)
# add_link_options("-rdynamic")

option(SVF_COVERAGE "Create coverage build")
if(SVF_COVERAGE OR DEFINED ENV{SVF_COVERAGE})
  add_compile_options("-fprofile-arcs" "-ftest-coverage")
  add_link_options("-fprofile-arcs" "-ftest-coverage")
  message(STATUS "Enable coverage")
endif()

set(SVF_SANITIZE
    ""
    CACHE STRING "Create sanitizer build (address)")
if(SVF_SANITIZE STREQUAL "address")
  add_compile_options("-fno-omit-frame-pointer" "-fsanitize=address")
  add_link_options("-fsanitize=address")
  message(STATUS "Sanitizer build: ${SVF_SANITIZE}")
elseif(SVF_SANITIZE STREQUAL "thread")
  add_compile_options("-fsanitize=thread")
  add_link_options("-fsanitize=thread")
  message(STATUS "Sanitizer build: ${SVF_SANITIZE}")
elseif(NOT SVF_SANITIZE STREQUAL "")
  message(ERROR "Unknown sanitizer type: ${SVF_SANITIZE}")
endif()

find_library(
  Z3_LIBRARIES
  NAMES z3
  HINTS ${Z3_DIR} ENV Z3_DIR
  PATH_SUFFIXES bin lib)
find_path(
  Z3_INCLUDES
  NAMES z3++.h
  HINTS ${Z3_DIR} ENV Z3_DIR
  PATH_SUFFIXES include z3)
if(NOT Z3_LIBRARIES OR NOT Z3_INCLUDES)
  message(FATAL_ERROR "Z3 not found!")
endif()
message(STATUS "Found Z3: ${Z3_LIBRARIES}")
message(STATUS "Z3 include dir: ${Z3_INCLUDES}")

include_directories(${PROJECT_SOURCE_DIR}/svf/include
                    ${PROJECT_BINARY_DIR}/include ${Z3_INCLUDES})

# checks if the test-suite is present, if it is then build bc files and add
# testing to cmake build
if(EXISTS "${PROJECT_SOURCE_DIR}/Test-Suite")
  include_directories(${PROJECT_SOURCE_DIR}/Test-Suite)
  enable_testing()
  add_subdirectory(Test-Suite)
  include(CTest)
endif()

add_subdirectory(svf)
add_subdirectory(svf-llvm)

include(GNUInstallDirs)
install(
  TARGETS SvfCore SvfLLVM
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/svf/include/
            ${PROJECT_SOURCE_DIR}/svf-llvm/include/
  COMPONENT devel
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/include/svf
  FILES_MATCHING
  PATTERN "**/*.h")

# Compile extapi.c to extapi.bc
find_path(LLVM_CLANG_DIR
        NAMES clang llvm
        HINTS ${LLVM_DIR} ENV LLVM_DIR
        PATH_SUFFIXES bin)
add_custom_target(extapi_ir ALL
    COMMAND ${LLVM_CLANG_DIR}/clang -w -S -c -Xclang -disable-O0-optnone -fno-discard-value-names -emit-llvm ${PROJECT_SOURCE_DIR}/svf-llvm/lib/extapi.c -o ${PROJECT_BINARY_DIR}/svf-llvm/extapi.bc
    DEPENDS ${PROJECT_SOURCE_DIR}/svf-llvm/lib/extapi.c
)