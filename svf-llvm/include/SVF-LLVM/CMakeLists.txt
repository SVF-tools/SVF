# Collect all .h (and .hpp) files and add them to the headers fileset of the SvfLLVM package
file(GLOB_RECURSE SVF_LLVM_HEADERS *.h *.hpp)
target_sources(
    SvfLLVM
    PUBLIC FILE_SET
           HEADERS
           BASE_DIRS
           ${SVF_LLVM_INCLUDES}
           FILES
           ${SVF_LLVM_HEADERS}
)
