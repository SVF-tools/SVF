# To support both in- and out-of-source builds, we check for the presence of the
# add_llvm_loadable_module command. - if this command is not present, we are
# building out-of-source
if(NOT COMMAND add_llvm_library)
  find_package(LLVM REQUIRED CONFIG HINTS "${LLVM_DIR}")

  message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

  if(NOT LLVM_ENABLE_RTTI)
    add_compile_options("-fno-rtti")
    message(STATUS "Disable RTTI")
  endif()

  if(NOT LLVM_ENABLE_EH)
    add_compile_options("-fno-exceptions")
    message(STATUS "Disable exceptions")
  endif()

  list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
  include(AddLLVM)

  add_definitions(${LLVM_DEFINITIONS})
  # include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

  if(LLVM_LINK_LLVM_DYLIB)
    set(llvm_libs LLVM)
  else()
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
else()
  set(IN_SOURCE_BUILD 1)
endif()

# SVF-LLVM containes LLVM Libs
file(GLOB SVFLLVM_SOURCES lib/*.cpp)

add_llvm_library(SvfLLVM STATIC ${SVFLLVM_SOURCES})
target_include_directories(SvfLLVM SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(SvfLLVM PUBLIC include)
target_link_libraries(SvfLLVM PUBLIC ${Z3_LIBRARIES} SvfCoreObj)

if(DEFINED IN_SOURCE_BUILD)
  add_dependencies(SvfLLVM intrinsics_gen)
endif()

add_subdirectory(tools)
