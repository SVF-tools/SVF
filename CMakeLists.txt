cmake_minimum_required(VERSION 3.13.4)

project("SVF")

configure_file(${CMAKE_SOURCE_DIR}/.config.in ${CMAKE_BINARY_DIR}/include/Util/config.h)

# To support both in- and out-of-source builds,
# we check for the presence of the add_llvm_loadable_module command.
# - if this command is not present, we are building out-of-source
if(NOT COMMAND add_llvm_library)
        # We need to match the build environment for LLVM:
        # In particular, we need C++14 and the -fno-rtti flag
        set(CMAKE_CXX_STANDARD 14)
        set(CMAKE_CXX_STANDARD_REQUIRED ON)
        # add -std=gnu++14
        set(CMAKE_CXX_EXTENSIONS ON)

        # Treat compiler warnings as errors
        add_compile_options("-Werror" "-Wall")

        find_package(LLVM REQUIRED CONFIG HINTS "${LLVM_DIR}")

        message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
        message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

        if (NOT LLVM_ENABLE_RTTI)
          add_compile_options("-fno-rtti")
          message(STATUS "Disable RTTI")
        endif()

        if (NOT LLVM_ENABLE_EH)
          add_compile_options("-fno-exceptions")
          message(STATUS "Disable exceptions")
        endif()

        option(SVF_COVERAGE "Create coverage build")
        if (SVF_COVERAGE OR DEFINED ENV{SVF_COVERAGE})
          add_compile_options("-fprofile-arcs" "-ftest-coverage")
          add_link_options("-fprofile-arcs" "-ftest-coverage")
          message(STATUS "Enable coverage")
        endif()

        list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
        include(AddLLVM)

        add_definitions(${LLVM_DEFINITIONS})
        include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

        if (LLVM_LINK_LLVM_DYLIB)
            set(llvm_libs LLVM)
        else()
            llvm_map_components_to_libnames(llvm_libs bitwriter core ipo irreader instcombine instrumentation target linker analysis scalaropts support transformutils)
        endif()
else()
        set(IN_SOURCE_BUILD 1)
endif()

find_library(Z3_LIBRARIES NAMES z3
                          HINTS ${Z3_DIR} ENV Z3_DIR
                          PATH_SUFFIXES bin lib)
find_path(Z3_INCLUDES NAMES z3++.h
                      HINTS ${Z3_DIR} ENV Z3_DIR
                      PATH_SUFFIXES include z3)
if(NOT Z3_LIBRARIES OR NOT Z3_INCLUDES)
    message(FATAL_ERROR "Z3 not found!")
endif()
message(STATUS "Found Z3: ${Z3_LIBRARIES}")
message(STATUS "Z3 include dir: ${Z3_INCLUDES}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include
                    ${CMAKE_BINARY_DIR}/include
                    ${Z3_INCLUDES})

# checks if the test-suite is present, if it is then build bc files and add testing to cmake build
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Test-Suite")
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Test-Suite)
    enable_testing()
    add_subdirectory(Test-Suite)
    include(CTest)
endif()

add_subdirectory(lib)
add_subdirectory(tools)

INSTALL(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    COMPONENT devel
    DESTINATION include/svf
    FILES_MATCHING
    PATTERN "**/*.h"
    )
