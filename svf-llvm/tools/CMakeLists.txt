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
    wpa)

foreach(_tool IN LISTS ALL_TOOLS)
    set_target_properties(
        ${_tool}
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}"
                   LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}"
                   ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")
    target_include_directories(${_tool} PRIVATE "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>")
    install(
        TARGETS ${_tool}
        EXPORT SVF_EXPORTED_TARGETS
        RUNTIME DESTINATION "${SVF_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${SVF_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${SVF_INSTALL_LIBDIR}")
endforeach()
