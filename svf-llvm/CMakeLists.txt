# Find the LLVM instance to use for building SvfLLVM
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

# Though not necessary, check if SVF is being built in debug mode & if that matches LLVM
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT ${LLVM_BUILD_TYPE} STREQUAL "Debug")
  message(NOTICE  "Building SVF in debug-mode but LLVM was not built in debug-mode; "
                  "debug information could be incomplete when using SVF from LLVM")
endif()

# Add LLVM's include directories and link directory for all targets defined hereafter
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Ensure SVF is built with RTTI/exception handling if the used LLVM instance has them enabled
set(SVF_ENABLE_RTTI ${LLVM_ENABLE_RTTI} PARENT_SCOPE)
set(SVF_ENABLE_EXCEPTIONS ${LLVM_ENABLE_EH} PARENT_SCOPE)

# Check if LLVM was built generating the single libLLVM.so shared library file or as separate static libraries
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
    demangle
  )
endif()

# Make the "add_llvm_library()" command available and configure LLVM/CMake
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# Define the actual SvfLLVM library (use LLVM's functions to automatically link stuff)
add_llvm_library(SvfLLVM ${SVFLLVM_SOURCES})

# Add the public headers as an include directory
target_include_directories(SvfLLVM
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${SVF_INSTALL_INCLUDE_DIR}>
)

set_target_properties(SvfLLVM PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Link LLVM's libraries to SvfLLVM, as well as the SVF core library
target_link_libraries(SvfLLVM PUBLIC ${llvm_libs} SvfCore)

# Get the source files (i.e. all *.c/*.cpp files) for SVF's subprojects and add them to SvfLLVM
file(GLOB_RECURSE SVF_LLVM_SOURCES lib/*.cpp)
target_sources(SvfLLVM PRIVATE ${SVF_LLVM_SOURCES})

# Get all of the public header files (i.e. all *.h/*.hpp files) for SVF's subprojects and add them
file(GLOB_RECURSE SVF_LLVM_HEADERS include/*.h include/*.hpp)

target_sources(SvfLLVM
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES ${SVF_LLVM_HEADERS}
)

# Add intrinsics_gen target if we're building as part of LLVM source build
if(TARGET intrinsics_gen)
  add_dependencies(SvfLLVM intrinsics_gen)
endif()

# Ensure extapi.bc built before libSvfLLVM such that
# we don't have to build all when building individual tools (e.g., wpa)
add_dependencies(SvfLLVM gen_extapi_ir)

# Add the targets for compiling the SvfLLVM tool binaries
add_subdirectory(tools)

# Find clang from the LLVM instance found earlier
find_program(LLVM_CLANG
  NAMES clang
  HINTS ${LLVM_BINARY_DIR}
  PATH_SUFFIXES bin
  REQUIRED
)

# Define a custom command which generates the "extapi.bc" LLVM bytecode file
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/extapi.bc
  COMMAND ${LLVM_CLANG} -w
                        -S
                        -c
                        -emit-llvm
                        -fno-discard-value-names
                        -Xclang -disable-O0-optnone
                        -o ${CMAKE_BINARY_DIR}/lib/extapi.bc
                        ${CMAKE_CURRENT_LIST_DIR}/lib/extapi.c
  DEPENDS ${CMAKE_CURRENT_LIST_DIR}/lib/extapi.c
)

# Add a custom target for generating the LLVM bytecode file (and add it to the default build targets)
add_custom_target(gen_extapi_ir ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/extapi.bc)

# Install the SvfLLVM shared library and public headers (public headers go in include/SVF-LLVM)
install(
  TARGETS SvfLLVM
  EXPORT SVFTargets
  RUNTIME DESTINATION ${SVF_INSTALL_BIN_DIR}
  LIBRARY DESTINATION ${SVF_INSTALL_LIB_DIR}
  ARCHIVE DESTINATION ${SVF_INSTALL_LIB_DIR}
  FILE_SET HEADERS DESTINATION ${SVF_INSTALL_INCLUDE_DIR}
)

# Install the generated extapi.bc bytecode file
install(
  FILES ${CMAKE_BINARY_DIR}/lib/extapi.bc
  DESTINATION ${SVF_INSTALL_EXTAPI_DIR}
)
