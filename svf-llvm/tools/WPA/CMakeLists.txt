set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(DEFINED IN_SOURCE_BUILD)
  add_llvm_tool(wpa wpa.cpp)
else()
  add_executable(wpa wpa.cpp)

  target_link_libraries(wpa SvfCore SvfLLVM Threads::Threads)

  separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
  target_include_directories(wpa PUBLIC include)
  target_include_directories(wpa SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
  target_link_directories(wpa PUBLIC ${LLVM_LIBRARY_DIRS})
  target_compile_definitions(wpa PUBLIC ${LLVM_DEFINITIONS})

  set_target_properties(wpa PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                       ${CMAKE_BINARY_DIR}/bin)
endif()
