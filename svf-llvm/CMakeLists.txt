# SVF-LLVM contains LLVM Libs
file(GLOB SVFLLVM_SOURCES lib/*.cpp)

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

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

if(NOT LLVM_ENABLE_EH)
  message(STATUS "Building SVF without exception handling")
  set(SVF_NO_RTTI ON PARENT_SCOPE)  # Set for parent scope because this needs to be set in top-level CMakeLists.txt
endif()

if(NOT LLVM_ENABLE_RTTI)
  message(STATUS "Building SVF without RTTI")
  set(SVF_NO_EH ON PARENT_SCOPE)  # Set for parent scope because this needs to be set in top-level CMakeLists.txt
endif()

if(LLVM_LINK_LLVM_DYLIB)
  message(STATUS "Linking to LLVM dynamic shared library object")
  set(llvm_libs LLVM)

  # Set which components to include in the dynamic library to include the new SvfLLVM
  if (LLVM_DYLIB_COMPONENTS)
    message(STATUS "Appending SvfLLVM to LLVM dynamic library components")
    list(APPEND LLVM_DYLIB_COMPONENTS SvfLLVM)
  else()
    message(STATUS "Adding all;SvfLLVM to LLVM dynamic library components (was unset)")
    set(LLVM_DYLIB_COMPONENTS all;SvfLLVM)
  endif()
else()
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
    transformutils
)
endif()

# Make the "add_llvm_library()" command available and configure LLVM/CMake
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_llvm_library(SvfLLVM ${SVFLLVM_SOURCES})
target_include_directories(SvfLLVM PUBLIC include)
target_link_libraries(SvfLLVM PUBLIC ${llvm_libs} ${Z3_LIBRARIES} SvfCore)

# Add intrinsics_gen target if we're building as part of LLVM source build
if(TARGET intrinsics_gen)
  add_dependencies(SvfLLVM intrinsics_gen)
endif()

add_subdirectory(tools)

# Compile extapi.c to extapi.bc
set(EXTAPI_TO_BC_FLAGS
  "-w"
  "-S"
  "-c"
  "-Xclang"
  "-disable-O0-optnone"
  "-fno-discard-value-names"
  "-emit-llvm"
  "${CMAKE_CURRENT_LIST_DIR}/lib/extapi.c"
  "-o"
  "${CMAKE_BINARY_DIR}/svf-llvm/extapi.bc"
)
find_program(LLVM_CLANG
  NAMES clang
  HINTS ${LLVM_BINARY_DIR}
  PATH_SUFFIXES bin
  REQUIRED
)
add_custom_target(extapi_ir ALL
  COMMAND ${LLVM_CLANG} ${EXTAPI_TO_BC_FLAGS}
  DEPENDS ${CMAKE_CURRENT_LIST_DIR}/lib/extapi.c
)

# Install the headers (as a subdirectory of the main SVF include) and the compiled library binaries
include(GNUInstallDirs)
install(
  TARGETS SvfLLVM
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(
  FILES ${CMAKE_BINARY_DIR}/svf-llvm/extapi.bc
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/svf/SVF-LLVM
)
install(
  DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/svf
  FILES_MATCHING
  PATTERN "**/*.h"
)

