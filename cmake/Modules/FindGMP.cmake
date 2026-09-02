# Find the GMP C and C++ libraries.

include(FindPackageHandleStandardArgs)

set(_GMP_HINTS ${GMP_ROOT} $ENV{GMP_ROOT} ${GMP_DIR} $ENV{GMP_DIR})

find_path(GMP_INCLUDE_DIR NAMES gmp.h gmpxx.h
          HINTS ${_GMP_HINTS} PATH_SUFFIXES include)
find_library(GMP_LIBRARY NAMES gmp libgmp
             HINTS ${_GMP_HINTS} PATH_SUFFIXES lib lib64)
find_library(GMPXX_LIBRARY NAMES gmpxx libgmpxx
             HINTS ${_GMP_HINTS} PATH_SUFFIXES lib lib64)

find_package_handle_standard_args(
  GMP REQUIRED_VARS GMP_INCLUDE_DIR GMP_LIBRARY GMPXX_LIBRARY
)

if(GMP_FOUND)
  if(NOT TARGET GMP::GMP)
    add_library(GMP::GMP UNKNOWN IMPORTED)
    set_target_properties(
      GMP::GMP PROPERTIES
      IMPORTED_LOCATION "${GMP_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${GMP_INCLUDE_DIR}"
    )
  endif()
  if(NOT TARGET GMP::GMPXX)
    add_library(GMP::GMPXX UNKNOWN IMPORTED)
    set_target_properties(
      GMP::GMPXX PROPERTIES
      IMPORTED_LOCATION "${GMPXX_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${GMP_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES GMP::GMP
    )
  endif()
  set(GMP_INCLUDE_DIRS "${GMP_INCLUDE_DIR}")
  set(GMP_LIBRARIES GMP::GMPXX GMP::GMP)
endif()

mark_as_advanced(GMP_INCLUDE_DIR GMP_LIBRARY GMPXX_LIBRARY)
