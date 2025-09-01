# =================================================================================
# LLVM package definition and configuration
# =================================================================================

# Find the LLVM instance to build & link SvfLLVM against (prioritise $LLVM_DIR)
find_package(LLVM CONFIG REQUIRED HINTS ${LLVM_HOME} $ENV{LLVM_HOME} ${LLVM_DIR} $ENV{LLVM_DIR})

# Import certain utilities (e.g. to make add_llvm_library() available)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# If exceptions are disabled, verify the LLVM instance could never raise them
if(LLVM_ENABLE_EH AND NOT SVF_ENABLE_EXCEPTIONS)
  message(WARNING "LLVM could throw exceptions but SVF configured to disable exception handling; "
                  "forcing $SVF_ENABLE_EXCEPTIONS to ON!"
  )
  set(SVF_ENABLE_EXCEPTIONS
      ON
      CACHE BOOL "" FORCE
  )
endif()

# If RTTI is disabled in LLVM, it must be for SVF as well (and vice versa)
if(LLVM_ENABLE_RTTI AND NOT SVF_ENABLE_RTTI)
  message(WARNING "LLVM is built with RTTI support but SVF is configured to disable RTTI support; "
                  "forcing $SVF_ENABLE_RTTI to ON!"
  )
  set(SVF_ENABLE_RTTI
      ON
      CACHE BOOL "" FORCE
  )
elseif(NOT LLVM_ENABLE_RTTI AND SVF_ENABLE_RTTI)
  message(WARNING "LLVM is built without RTTI support but SVF configured to enable RTTI support; "
                  "forcing $SVF_ENABLE_RTTI to OFF!"
  )
  set(SVF_ENABLE_RTTI
      OFF
      CACHE BOOL "" FORCE
  )
endif()

# Make the include/link directories & definitions available globally
separate_arguments(_LLVM_DEFINITIONS NATIVE_COMMAND ${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${_LLVM_DEFINITIONS})

# Use all components as link targets (picks dynamic/static libs); store in $LLVM_LIBRARIES
if(LLVM_LINK_LLVM_DYLIB)
  set(LLVM_LIBRARIES LLVM)
else()
  llvm_map_components_to_libnames(LLVM_LIBRARIES all)
endif()

# Search in the executables dir for this LLVM's clang instance
find_program(LLVM_CLANG clang ${LLVM_BINARY_DIR} ${LLVM_TOOLS_BINARY_DIR})
if(NOT LLVM_CLANG)
  message(FATAL_ERROR "Failed to find 'clang' in LLVM package!")
endif()

message(
  STATUS
    "Using LLVM package:
  LLVM major version:         ${LLVM_VERSION_MAJOR}
  LLVM minor version:         ${LLVM_VERSION_MINOR}
  LLVM patch version:         ${LLVM_VERSION_PATCH}
  LLVM version string:        ${LLVM_PACKAGE_VERSION}
  LLVM host triple:           ${LLVM_HOST_TRIPLE}
  LLVM target triple:         ${LLVM_TARGET_TRIPLE}
  LLVM build type:            ${LLVM_BUILD_TYPE}
  LLVM link dynamic lib:      ${LLVM_LINK_LLVM_DYLIB}
  LLVM enable RTTI:           ${LLVM_ENABLE_RTTI}
  LLVM enable exceptions:     ${LLVM_ENABLE_EH}
  LLVM compiler definitions:  ${LLVM_DEFINITIONS}
  LLVM libraries:             ${LLVM_LIBRARIES}
  LLVM includes dir:          ${LLVM_INCLUDE_DIRS}
  LLVM libraries dir:         ${LLVM_LIBRARY_DIRS}
  LLVM executables dir:       ${LLVM_BINARY_DIR}
  LLVM executable tools dir:  ${LLVM_TOOLS_BINARY_DIR}"
)

# =================================================================================
# ExtAPI.bc generation & target definition
# =================================================================================
set(EXTAPI_SRC ${CMAKE_CURRENT_LIST_DIR}/lib/extapi.c)

if(EXISTS "${EXTAPI_SRC}")
  message(STATUS "Found extapi.c input file at: ${EXTAPI_SRC}")
else()
  message(FATAL_ERROR "Failed to find extapi.c input file: ${EXTAPI_SRC}")
endif()

# Add a custom command to compile the extapi.bc bitcode file from extapi.c
add_custom_command(
  OUTPUT ${SVF_BUILD_EXTAPI_BC}
  COMMAND ${LLVM_CLANG} -w -S -c -fPIC -std=gnu11 -emit-llvm -o ${SVF_BUILD_EXTAPI_BC} ${EXTAPI_SRC}
  COMMENT "Generating extapi.bc LLVM bitcode file..."
  MAIN_DEPENDENCY ${EXTAPI_SRC}
  DEPENDS ${EXTAPI_SRC}
)

# Add a custom target that has the custom command as its creation command
add_custom_target(gen_extapi_ir ALL DEPENDS ${SVF_BUILD_EXTAPI_BC})

# Install the bitcode file as well; install it to (default) <prefix>/lib/extapi.bc
install(
  FILES ${SVF_BUILD_EXTAPI_BC}
  DESTINATION ${SVF_INSTALL_EXTAPIDIR}
  RENAME ${SVF_EXTAPI_BC_NAME}
)

# =================================================================================
# Main SVF LLVM library definition
# =================================================================================

# Define SVF's LLVM library
add_library(SvfLLVM)

# If SVF is built as part of the LLVM source tree, explicitly add a dependency on the intrinsics_gen target
if(TARGET intrinsics_gen)
  add_dependencies(SvfLLVM intrinsics_gen)
endif()

# Add a dependency from the SvfLLVM library to the extapi.bc target (so it's generated when SvfLLVM is built)
add_dependencies(SvfLLVM gen_extapi_ir)

# Link the SVF LLVM library to the core library, the build/link flags, and the LLVM libraries
target_link_libraries(SvfLLVM PUBLIC SvfFlags ${LGRAPH_CPP_CLIENT_LIBRARIES} SvfCore ${LLVM_LIBRARIES})

# Gather & set all of the core library's source files by globbing all .h and .cpp files (recursively)
file(GLOB_RECURSE SVF_LLVM_HEADERS ${CMAKE_CURRENT_LIST_DIR}/include/*.h)
file(GLOB_RECURSE SVF_LLVM_SOURCES ${CMAKE_CURRENT_LIST_DIR}/lib/*.cpp)
target_sources(
  SvfLLVM
  PUBLIC FILE_SET HEADERS
                  BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}/include
                  FILES ${SVF_LLVM_HEADERS}
  PRIVATE ${SVF_LLVM_SOURCES}
)

# Only expose the headers in the source tree to in-tree users of SVF
target_include_directories(SvfLLVM PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)

# Set the library & .so version of the LLVM library
set_target_properties(SvfLLVM PROPERTIES VERSION ${SVF_VERSION} SOVERSION ${SVF_VERSION_MAJOR})

# Install the LLVM library's files to the regular/configured install tree
install(
  TARGETS SvfLLVM
  EXPORT SVFTargets
  RUNTIME DESTINATION ${SVF_INSTALL_BINDIR}
  LIBRARY DESTINATION ${SVF_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${SVF_INSTALL_LIBDIR}
          FILE_SET HEADERS
          DESTINATION ${SVF_INSTALL_INCLUDEDIR}
)

# =================================================================================
# Tools
# =================================================================================
add_subdirectory(tools)
