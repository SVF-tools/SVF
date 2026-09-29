add_subdirectory(SABER)
add_subdirectory(WPA)
add_subdirectory(Example)
add_subdirectory(DDA)
add_subdirectory(MTA)
add_subdirectory(CFL)
add_subdirectory(LLVM2SVF)
add_subdirectory(AE)

set(ALL_TOOLS
    ae
    cfl
    dvf
    llvm2svf
    mta
    saber
    svf-ex
    wpa
)

foreach(_tool IN LISTS ALL_TOOLS)
  # Link the SvfLLVM library against SVF's core & LLVM libraries
  target_link_libraries(${_tool} PRIVATE SvfCore SvfLLVM)

  # On Windows, increaseStackSize() is a no-op (RLIMIT_STACK not available).
  # Set the PE stack reserve to 256 MB at link time to match Linux behaviour.
  if(WIN32)
    if(MSVC OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
      if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
        # clang++ targeting MSVC ABI
        target_link_options(${_tool} PRIVATE -Xlinker /STACK:268435456)
      else()
        # cl.exe or clang-cl.exe targeting MSVC ABI
        target_link_options(${_tool} PRIVATE /STACK:268435456)
      endif()
    else()
      # MinGW (clang++ or g++) uses GNU linker syntax
      target_link_options(${_tool} PRIVATE -Wl,--stack,268435456)
    endif()
  endif()

  # Ensure that the build artifacts are placed into lib/bin/include relative to the top of the build directory
  set_target_properties(
    ${_tool}
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${SVF_BINARY_DIR}/bin
               LIBRARY_OUTPUT_DIRECTORY ${SVF_BINARY_DIR}/lib
               ARCHIVE_OUTPUT_DIRECTORY ${SVF_BINARY_DIR}/lib
  )

  install(
    TARGETS ${_tool}
    EXPORT SVFTargets
    RUNTIME DESTINATION ${SVF_INSTALL_BINDIR}
    LIBRARY DESTINATION ${SVF_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${SVF_INSTALL_LIBDIR}
            FILE_SET HEADERS
            DESTINATION ${SVF_INSTALL_INCLUDEDIR}
  )
endforeach()
