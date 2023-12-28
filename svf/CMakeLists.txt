# Due to a mutual dependencies, all the sub projects of the SVG are merged here
# Otherwise it is impossible to load the dependencies in opt.
# NOTE: if the SVF should be linked into opt, we should probably use the
# individual sub projects here, rather than the combined project.

file(
  GLOB
  SVF_CORE_SOURCES
  lib/AbstractExecution/*.cpp
  lib/CFL/*.cpp
  lib/DDA/*.cpp
  lib/FastCluster/*.cpp
  lib/Graphs/*.cpp
  lib/MemoryModel/*.cpp
  lib/MSSA/*.cpp
  lib/MTA/*.cpp
  lib/SABER/*.cpp
  lib/SVFIR/*.cpp
  lib/Util/*.cpp
  lib/WPA/*.cpp)

add_library(SvfCore ${SVF_CORE_SOURCES})

target_link_libraries(SvfCore PUBLIC ${Z3_LIBRARIES})

include(GNUInstallDirs)
install(
  TARGETS SvfCore
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(
  DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/svf
  FILES_MATCHING
  PATTERN "**/*.h")
