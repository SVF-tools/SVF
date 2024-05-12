# Collect all .h (and .hpp) files and add them to the headers fileset of the SvfLLVM package
file(GLOB_RECURSE _LOCAL_HEADERS *.h *.hpp)
message(STATUS "Found local headers in ${CMAKE_CURRENT_LIST_DIR}: ${_LOCAL_HEADERS}")

# Add the headers to the sources of libSvfLLVM; note the base directory
# in the fileset points to the <SVF_DIR>/svf/include directory
target_sources(
    SvfLLVM
    PUBLIC FILE_SET
           HEADERS
           BASE_DIRS
           ${SVF_LLVM_INCLUDES}
           FILES
           ${_LOCAL_HEADERS})
