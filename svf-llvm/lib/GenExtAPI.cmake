# Set base variables and input/output structures
set(SVF_EXTAPI_FLAGS
    -w
    -S
    -c
    -fPIC
    -std=gnu11
    -emit-llvm
    -Xclang
    -disable-O0-optnone)

set(EXTAPI_SRC_FILE "${CMAKE_CURRENT_LIST_DIR}/extapi.c")
set(EXTAPI_BIN_FILE "${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/extapi.bc")

if(NOT ${SVF_ENABLE_OPAQUE_POINTERS})
    list(APPEND SVF_EXTAPI_FLAGS -Xclang -no-opaque-pointers)
endif()

if(NOT EXISTS "${EXTAPI_SRC_FILE}")
    message(FATAL_ERROR "Failed to find ExtAPI source file!")
endif()

# Find the Clang compiler; should be available through the LLVM instance found earlier
find_program(
    LLVM_CLANG
    NAMES clang
    HINTS "${LLVM_BINARY_DIR}"
    PATH_SUFFIXES "bin" REQUIRED)

# Define a custom command to compile extapi.c to LLVM IR bitcode in extapi.bc (in the build directory)
add_custom_command(
    OUTPUT "${EXTAPI_BIN_FILE}"
    COMMAND ${LLVM_CLANG} ${SVF_EXTAPI_FLAGS} -o ${EXTAPI_BIN_FILE} ${EXTAPI_SRC_FILE}
    DEPENDS ${EXTAPI_SRC_FILE})

# Add a custom target for generating the LLVM bytecode file (and add it to the default build targets)
add_custom_target(gen_extapi_ir ALL DEPENDS "${EXTAPI_BIN_FILE}")

# Install the bitcode file as well; install it to <prefix>/lib/extapi.bc
install(FILES "${EXTAPI_BIN_FILE}" DESTINATION "${SVF_INSTALL_LIBDIR}")
