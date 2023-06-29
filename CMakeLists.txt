cmake_minimum_required(VERSION 3.13.4)

project("SVF")

configure_file(${PROJECT_SOURCE_DIR}/.config.in
               ${PROJECT_BINARY_DIR}/include/Util/config.h)

# We need to match the build environment for LLVM: In particular, we need C++14
# and the -fno-rtti flag
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# add -std=gnu++14
set(CMAKE_CXX_EXTENSIONS ON)

add_compile_options("-fno-rtti")
add_compile_options("-fno-exceptions")

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

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/svf/include/
            ${PROJECT_SOURCE_DIR}/svf-llvm/include/
  COMPONENT devel
  DESTINATION include/svf
  FILES_MATCHING
  PATTERN "**/*.h")
