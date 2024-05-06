# Grab all .cpp source files in this directory and add them to the SvfCore package
file(GLOB_RECURSE SVF_CORE_MSSA_SOURCES *.cpp)
target_sources(SvfCore PRIVATE ${SVF_CORE_MSSA_SOURCES})
