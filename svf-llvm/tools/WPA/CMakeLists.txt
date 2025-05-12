set(THREADS_PREFER_PTHREAD_FLAG ON)

add_llvm_executable(wpa wpa.cpp)

find_package(Threads REQUIRED)
target_link_libraries(wpa PUBLIC Threads::Threads)
