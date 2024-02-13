set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_llvm_executable(wpa wpa.cpp)
target_link_libraries(wpa PUBLIC ${llvm_libs} SvfLLVM Threads::Threads)
