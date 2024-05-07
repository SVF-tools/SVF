# Find the LLVM instance to use for building SvfLLVM (makes add_llvm_library() available)
find_package(
    LLVM
    REQUIRED
    CONFIG
    HINTS
    ${LLVM_DIR}
    $ENV{LLVM_DIR})
message(STATUS "LLVM STATUS:
        Version       ${LLVM_VERSION}
        Definitions   ${LLVM_DEFINITIONS}
        Includes      ${LLVM_INCLUDE_DIRS}
        Libraries     ${LLVM_LIBRARY_DIRS}
        Targets       ${LLVM_TARGETS_TO_BUILD}
        Build type    ${LLVM_BUILD_TYPE}
        Exceptions    ${LLVM_ENABLE_EH}
        RTTI          ${LLVM_ENABLE_RTTI}
        Dynamic lib   ${LLVM_LINK_LLVM_DYLIB}")

# Though not necessary, check if SVF is being built in debug mode & if that matches LLVM
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT ${LLVM_BUILD_TYPE} STREQUAL "Debug")
    message(NOTICE "Building SVF in debug-mode but LLVM was not built in debug-mode; "
            "debug information could be incomplete when using SVF from LLVM")
endif()

# Add LLVM's include directories and link directory for all targets defined hereafter
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
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# Define the actual SvfLLVM library (use LLVM's functions to automatically link stuff)
add_llvm_library(SvfLLVM)
add_library(SVF::LLVM ALIAS SvfLLVM)
set_target_properties(SvfLLVM PROPERTIES VERSION ${SVF_VERSION} SOVERSION ${SVF_VERSION_MAJOR})

# Add the public headers as an include directory
target_include_directories(SvfLLVM
                           PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
                                  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                  $<INSTALL_INTERFACE:${SVF_INCLUDE_DIR}>)

# Link LLVM's libraries to SvfLLVM, as well as the SVF core library
target_link_libraries(SvfLLVM PUBLIC ${llvm_libs} SvfCore)

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
    EXPORT SVFTargets
    RUNTIME DESTINATION ${SVF_INSTALL_BINDIR}
    LIBRARY DESTINATION ${SVF_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${SVF_INSTALL_LIBDIR}
            FILE_SET HEADERS
            DESTINATION ${SVF_INSTALL_INCLUDEDIR})
