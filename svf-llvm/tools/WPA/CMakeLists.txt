set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(DEFINED IN_SOURCE_BUILD)
  add_llvm_tool(wpa wpa.cpp)
else()
  add_executable(wpa wpa.cpp)

  target_link_libraries(wpa SvfLLVM ${llvm_libs} Threads::Threads)

  set_target_properties(wpa PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                       ${CMAKE_BINARY_DIR}/bin)
endif()
