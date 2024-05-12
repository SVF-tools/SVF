if(DEFINED LLVM_ROOT OR DEFINED ENV{LLVM_ROOT})
    if(NOT DEFINED LLVM_ROOT)
        set(LLVM_ROOT $ENV{LLVM_ROOT})
    endif()

    message(STATUS "Finding LLVM in custom LLVM root: ${LLVM_ROOT}")
    if(EXISTS "${LLVM_ROOT}/lib/cmake/llvm")
        list(APPEND CMAKE_PREFIX_PATH ${LLVM_ROOT}/lib/cmake/llvm/)
    else()
        list(APPEND CMAKE_PREFIX_PATH ${LLVM_ROOT})
    endif()
endif()

# Find the LLVM instance to use for building SvfLLVM (makes add_llvm_library() available)
find_package(
    LLVM
    CONFIG
    REQUIRED
    HINTS
    ${LLVM_DIR}
    $ENV{LLVM_DIR})

message(STATUS "LLVM STATUS:
        Found:          ${LLVM_FOUND}
        Version:        ${LLVM_VERSION}
        Definitions:    ${LLVM_DEFINITIONS}
        Includes:       ${LLVM_INCLUDE_DIRS}
        Libraries:      ${LLVM_LIBRARY_DIRS}
        Targets:        ${LLVM_TARGETS_TO_BUILD}
        Build type:     ${LLVM_BUILD_TYPE}
        Exceptions:     ${LLVM_ENABLE_EH}
        RTTI:           ${LLVM_ENABLE_RTTI}
        Dynamic lib:    ${LLVM_LINK_LLVM_DYLIB}")

# Though not necessary, check if SVF is being built in debug mode & if that matches LLVM
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT ${LLVM_BUILD_TYPE} STREQUAL "Debug")
    message(NOTICE "Building SVF in debug-mode but LLVM was not built in debug-mode; "
            "debug information could be incomplete when using SVF from LLVM")
endif()

# Add LLVM's include directories and link directory for all targets defined hereafter
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Ensure SVF is built with RTTI/exception handling if the used LLVM instance has them enabled
set(SVF_ENABLE_RTTI
    ${LLVM_ENABLE_RTTI}
    PARENT_SCOPE)
set(SVF_ENABLE_EXCEPTIONS
    ${LLVM_ENABLE_EH}
    PARENT_SCOPE)

# Check if LLVM was built generating the single libLLVM.so shared library file or as separate static libraries
if(LLVM_LINK_LLVM_DYLIB)
    message(STATUS "Linking to LLVM dynamic shared library object")
    set(llvm_libs LLVM)

    # Set which components to include in the dynamic library to include the new SvfLLVM
    if(LLVM_DYLIB_COMPONENTS)
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
        transformutils)
endif()

# Make the "add_llvm_library()" command available and configure LLVM/CMake
list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})
include(AddLLVM)

# Define the actual SvfLLVM library (use LLVM's functions to automatically link stuff)
add_llvm_library(SvfLLVM)
add_library(SVF::SvfLLVM ALIAS SvfLLVM)
set_target_properties(SvfLLVM PROPERTIES VERSION ${SVF_VERSION} SOVERSION ${SVF_VERSION_MAJOR})

# Add the public headers as an include directory
target_include_directories(SvfLLVM PRIVATE ${PROJECT_BINARY_DIR}/include)
target_include_directories(SvfLLVM PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                          $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/SVF>)

# Ensure that the build artifacts are placed into lib/bin/include relative to the top of the build directory
set_target_properties(
    SvfLLVM
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}
               ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}
               RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})

# Link LLVM's libraries to SvfLLVM, as well as the SVF core library
target_link_libraries(SvfLLVM PUBLIC SvfCore ${llvm_libs} z3::libz3)

# Add intrinsics_gen target if we're building as part of LLVM source build
if(TARGET intrinsics_gen)
    add_dependencies(SvfLLVM intrinsics_gen)
endif()

# Sources and headers are defined in the subdirectories
add_subdirectory(lib)
add_subdirectory(include)

# Add the targets for compiling the SvfLLVM tool binaries
add_subdirectory(tools)

# Install the SvfLLVM shared library and public headers (public headers go in include/SVF-LLVM)
install(
    TARGETS SvfLLVM
    EXPORT SVF_EXPORTED_TARGETS
    LIBRARY DESTINATION ${SVF_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${SVF_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${SVF_INSTALL_BINDIR}
            FILE_SET HEADERS
            DESTINATION ${SVF_INSTALL_INCLUDEDIR})
