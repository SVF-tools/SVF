# Collect all .h (and .hpp) files and add them to the headers fileset of the SvfCore package
file(GLOB_RECURSE SVF_FASTCLUSTER_HEADERS *.h *.hpp)
target_sources(
    SvfCore
    PUBLIC FILE_SET
           HEADERS
           BASE_DIRS
           ${SVF_CORE_INCLUDES}
           FILES
           ${SVF_FASTCLUSTER_HEADERS}
)
