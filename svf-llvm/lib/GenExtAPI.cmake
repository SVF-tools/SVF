# Set base variables and input/output structures
set(SVF_EXTAPI_SRC_NAME extapi.c)
set(SVF_EXTAPI_IN_FILE ${CMAKE_CURRENT_LIST_DIR}/${SVF_EXTAPI_SRC_NAME})
set(SVF_EXTAPI_OUT_FILE ${CMAKE_BINARY_DIR}/lib/${SVF_EXTAPI_OUT_NAME})
set(SVF_EXTAPI_FLAGS
    -w
    -S
    -c
    -fPIC
    -std=gnu11
    -emit-llvm
    -fno-discard-value-names
    -Xclang
    -disable-O0-optnone)

if(NOT ${SVF_ENABLE_OPAQUE_POINTERS})
    list(APPEND SVF_EXTAPI_FLAGS -Xclang -no-opaque-pointers)
endif()

if(NOT EXISTS ${SVF_EXTAPI_IN_FILE})
    message(FATAL_ERROR "Failed to find ExtAPI source file!")
endif()

# Find the Clang compiler; should be available through the LLVM instance found earlier
find_program(
    LLVM_CLANG
    NAMES clang
    HINTS ${LLVM_BINARY_DIR}
    PATH_SUFFIXES bin REQUIRED)

# Define a custom command to compile extapi.c to LLVM IR bitcode in extapi.bc (in the build directory)
add_custom_command(
    OUTPUT ${SVF_EXTAPI_OUT_FILE}
    COMMAND ${LLVM_CLANG} ${SVF_EXTAPI_FLAGS} -o ${SVF_EXTAPI_OUT_FILE} ${SVF_EXTAPI_IN_FILE}
    DEPENDS ${SVF_EXTAPI_IN_FILE})

# Add a custom target for generating the LLVM bytecode file (and add it to the default build targets)
add_custom_target(gen_extapi_ir ALL DEPENDS ${SVF_EXTAPI_OUT_FILE})

# Install the bitcode file as well; install it to <prefix>/lib/extapi.bc
install(FILES ${SVF_EXTAPI_OUT_FILE} DESTINATION ${SVF_INSTALL_LIBDIR})
