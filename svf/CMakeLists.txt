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

# Refer to this link for detail:
# https://stackoverflow.com/questions/68864536/cmake-linking-static-libraries-in-different-subdirectories-into-one-single-stati
add_library(SvfCoreObj OBJECT ${SVF_CORE_SOURCES})
add_library(SvfCore STATIC)
target_link_libraries(SvfCore PUBLIC ${Z3_LIBRARIES} SvfCoreObj)
