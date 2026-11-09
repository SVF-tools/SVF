# Find MPFR and expose MPFR::MPFR.

include(FindPackageHandleStandardArgs)

set(_MPFR_HINTS ${MPFR_ROOT} $ENV{MPFR_ROOT} ${MPFR_DIR} $ENV{MPFR_DIR})

find_path(MPFR_INCLUDE_DIR NAMES mpfr.h
          HINTS ${_MPFR_HINTS} PATH_SUFFIXES include)
find_library(MPFR_LIBRARY NAMES mpfr libmpfr
             HINTS ${_MPFR_HINTS} PATH_SUFFIXES lib lib64)

find_package_handle_standard_args(
  MPFR REQUIRED_VARS MPFR_INCLUDE_DIR MPFR_LIBRARY
)

if(MPFR_FOUND)
  if(NOT TARGET GMP::GMP)
    find_package(GMP REQUIRED)
  endif()
  if(NOT TARGET MPFR::MPFR)
    add_library(MPFR::MPFR UNKNOWN IMPORTED)
    set_target_properties(
      MPFR::MPFR PROPERTIES
      IMPORTED_LOCATION "${MPFR_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${MPFR_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES GMP::GMP
    )
  endif()
  set(MPFR_INCLUDE_DIRS "${MPFR_INCLUDE_DIR}")
  set(MPFR_LIBRARIES MPFR::MPFR)
endif()

mark_as_advanced(MPFR_INCLUDE_DIR MPFR_LIBRARY)
