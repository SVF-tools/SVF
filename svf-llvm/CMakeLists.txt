# =================================================================================
# LLVM package definition and configuration
# =================================================================================

if(DEFINED LLVM_ROOT OR DEFINED ENV{LLVM_ROOT})
    if(NOT DEFINED LLVM_ROOT)
        set(LLVM_ROOT $ENV{LLVM_ROOT})
    endif()

    message(STATUS "Finding LLVM in custom LLVM root: ${LLVM_ROOT}")
    if(EXISTS "${LLVM_ROOT}/lib/cmake/llvm")
        list(PREPEND CMAKE_PREFIX_PATH ${LLVM_ROOT}/lib/cmake/llvm/)
    else()
        list(PREPEND CMAKE_PREFIX_PATH ${LLVM_ROOT})
    endif()
endif()

# Find the LLVM instance to build & link SvfLLVM against
find_package(LLVM CONFIG REQUIRED HINTS ${LLVM_DIR} $ENV{LLVM_DIR})

# Find an instance of 'clang' in the found LLVM installation
find_program(
    LLVM_CLANG
    NAMES clang
    HINTS ${LLVM_BINARY_DIR}
    PATH_SUFFIXES bin REQUIRED
    )

# Make commands like "add_llvm_library" and such available
list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})
include(AddLLVM)

# Though not necessary, check if build modes match & report mismatches
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT ${LLVM_BUILD_TYPE} STREQUAL "Debug")
    message(NOTICE "Building SVF in debug-mode but LLVM was not built in debug-mode; "
            "debug information could be incomplete when using SVF from LLVM"
        )
endif()

# If the LLVM instance was built without RTTI, disable it globally by disabling it on the interface library
if(NOT LLVM_ENABLE_RTTI)
    message(NOTICE "LLVM was built without RTTI support; disabling RTTI...")
    target_compile_options(SvfFlags PUBLIC -fno-rtti)
    target_link_options(SvfFlags PUBLIC -fno-rtti)
endif()

# Test whether the current compiler supports handling exceptions
include(CheckCXXSourceCompiles)
check_cxx_source_compiles(
    "
int main() {
    try { throw 1; } catch (...) {}
    return 0;
}
"
    _SVF_COMPILER_SUPPORTS_EH
    )

# If LLVM could throw an exception but the compiler doesn't support them, abort
if(LLVM_ENABLE_EH AND NOT _SVF_COMPILER_SUPPORTS_EH)
    message(FATAL_ERROR "LLVM could throw exceptions but current compiler doesn't support exceptions!")
endif()

# Check if LLVM was built generating the single libLLVM.so shared library file or as separate static libraries
if(LLVM_LINK_LLVM_DYLIB)
    message(STATUS "Linking to LLVM dynamic shared library object")
    set(llvm_libs LLVM)

    # If SVF is built as part of the LLVM source tree, add it as a component of libLLVM.so
    if(LLVM_DYLIB_COMPONENTS)
        message(STATUS "Appending SvfLLVM to LLVM dynamic library components")
        list(APPEND LLVM_DYLIB_COMPONENTS SvfLLVM)
    else()
        message(STATUS "Adding all; SvfLLVM to LLVM dynamic library components (was unset)")
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

# =================================================================================
# ExtAPI.bc generation & target definition
# =================================================================================
set(EXTAPI_SRC_FILE ${CMAKE_CURRENT_LIST_DIR}/lib/extapi.c)

if(EXISTS "${EXTAPI_SRC_FILE}")
    message(STATUS "Found extapi.c input file at: ${EXTAPI_SRC_FILE}")
else()
    message(FATAL_ERROR "Failed to find extapi.c input file: ${EXTAPI_SRC_FILE}")
endif()

# Set the flags to compile extapi.c with
set(EXTAPI_COMPILE_FLAGS
    -w
    -S
    -c
    -fPIC
    -std=gnu11
    -emit-llvm
    -Xclang
    -disable-O0-optnone
    )

# Add a custom command to compile the extapi.bc bitcode file from extapi.c
add_custom_command(
    OUTPUT ${SVF_BUILD_EXTAPI_BC}
    COMMAND ${LLVM_CLANG} ${EXTAPI_COMPILE_FLAGS} -o ${SVF_BUILD_EXTAPI_BC} ${EXTAPI_SRC_FILE}
    DEPENDS ${EXTAPI_SRC_FILE}
    )

# Add a custom target that has the custom command as its creation command
add_custom_target(gen_extapi_ir ALL DEPENDS ${SVF_BUILD_EXTAPI_BC})

# Install the bitcode file as well; install it to (default) <prefix>/include/SVF/extapi.bc
install(
    FILES ${SVF_BUILD_EXTAPI_BC}
    DESTINATION ${SVF_INSTALL_EXTAPIDIR}
    RENAME ${SVF_EXTAPI_BC_NAME}
    )

# =================================================================================
# Main SVF LLVM library definition
# =================================================================================

# Define library
add_library(SvfLLVM)
add_library(SVF::LLVM ALIAS SvfLLVM)

# If SVF is built as part of the LLVM source tree, explicitly add a dependency on the intrinsics_gen target
if(TARGET intrinsics_gen)
    add_dependencies(SvfLLVM intrinsics_gen)
endif()

# Add a dependency from the SvfLLVM library to the extapi.bc target (so it's generated when SvfLLVM is built)
add_dependencies(SvfLLVM gen_extapi_ir)

# Inherit compiler & linker options by publically linking against the interface library
target_link_libraries(SvfLLVM PUBLIC SvfFlags)

# The SvfLLVM library depends on & links against the SvfCore library (public)
target_link_libraries(SvfLLVM PUBLIC SVF::Core)

# Add LLVM's defs to the public interface (SVF users will inherit) & link against libLLVM (also inherited)
target_compile_definitions(SvfLLVM PUBLIC ${LLVM_DEFINITIONS})
target_link_libraries(SvfLLVM PUBLIC ${llvm_libs})

# The include & link directories are absolute & should not be exposed in the public interface so limit to build
target_include_directories(SvfLLVM SYSTEM PUBLIC $<BUILD_INTERFACE:${LLVM_INCLUDE_DIRS}>)
target_link_directories(SvfLLVM PUBLIC $<BUILD_INTERFACE:${LLVM_LIBRARY_DIRS}>)

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

# Set & expose the include directories for the LLVM library
target_include_directories(
    SvfLLVM
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    )

# Set generic properties of the LLVM library (not exposed)
set_target_properties(
    SvfLLVM
    PROPERTIES CMAKE_CXX_EXTENSIONS ON
               VERSION ${SVF_VERSION} # Set SVF version
               SOVERSION ${SVF_VERSION_MAJOR} # Set shared lib's version
               POSITION_INDEPENDENT_CODE ${SVF_USE_PIC} # Enable PIC if configured
               INTERPROCEDURAL_OPTIMIZATION ${SVF_USE_LTO} # Enable LTO if configured
               RUNTIME_OUTPUT_DIRECTORY ${SVF_BINARY_DIR}/bin # Compile/output executables to build/bin
               LIBRARY_OUTPUT_DIRECTORY ${SVF_BINARY_DIR}/lib # Compile/output shared libraries to build/lib
               ARCHIVE_OUTPUT_DIRECTORY ${SVF_BINARY_DIR}/lib # Compile/output static libraries to build/lib
    )

# Install the LLVM library's files to the regular/configured install tree
install(
    TARGETS SvfLLVM
    EXPORT SVFTargets
    RUNTIME DESTINATION ${SVF_INSTALL_BINDIR}
    LIBRARY DESTINATION ${SVF_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${SVF_INSTALL_LIBDIR}
            FILE_SET HEADERS
            DESTINATION ${SVF_INSTALL_INCLUDEDIR}/SVF
    )

# =================================================================================
# Tools
# =================================================================================
add_subdirectory(tools)
