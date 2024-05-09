add_subdirectory(SABER)
add_subdirectory(WPA)
add_subdirectory(Example)
add_subdirectory(DDA)
add_subdirectory(MTA)
add_subdirectory(CFL)
add_subdirectory(LLVM2SVF)
add_subdirectory(AE)

set_target_properties(
    cfl
    dvf
    svf-ex
    llvm2svf
    mta
    saber
    wpa
    ae
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}"
               ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}"
               RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")

install(
    TARGETS cfl
            dvf
            svf-ex
            llvm2svf
            mta
            saber
            wpa
            ae
    EXPORT SVF_EXPORTED_TARGETS
    RUNTIME DESTINATION "${SVF_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${SVF_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${SVF_INSTALL_LIBDIR}")
