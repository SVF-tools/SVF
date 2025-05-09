#
#  FindZ3.cmake - Locate Z3 either via a packaged config or by header+lib
#

# Try upstream/system CONFIG package first; use it if found (sets required variables):
find_package(Z3 CONFIG QUIET HINTS ${Z3_DIR} $ENV{Z3_DIR})
if(Z3_FOUND)
  if(NOT Z3_FIND_QUIETLY)
    message(STATUS "Found upstream/system Z3 package (Z3Config.cmake)")
  endif()
else()
  if(NOT Z3_FIND_QUIETLY)
    message(STATUS "Failed to find upstream/system Z3 package; reverting to manual search")
  endif()

  # Fall back to explicit manual header + lib search (prioritise searching $Z3_DIR)
  find_library(Z3_LIBRARY_DIR NAMES z3 libz3 HINTS ${Z3_DIR} $ENV{Z3_DIR})
  find_path(Z3_INCLUDE_DIR NAMES z3++.h HINTS ${Z3_DIR} $ENV{Z3_DIR})

  # If the headers were found, find & extract the Z3 version number
  set(_ver_h "${Z3_INCLUDE_DIR}/z3_version.h")
  if(EXISTS "${_ver_h}")
    message(STATUS "Found Z3 version header file: ${_ver_h}")
    file(READ "${_ver_h}" _z3_ver_h)

    # Grab the full-version string
    string(REGEX MATCH "#define Z3_FULL_VERSION[ \t]+\"([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)\"" _ ${_z3_ver_h})
    set(Z3_VERSION_STRING "${CMAKE_MATCH_1}")

    # Split the full Z3 version string from the public header into its numeric components
    message(STATUS "Found Z3 headers for version ${Z3_VERSION_STRING}")
    string(REGEX MATCHALL "[0-9]+" _components "${Z3_VERSION_STRING}")
    list(GET _components 0 Z3_VERSION_MAJOR)
    list(GET _components 1 Z3_VERSION_MINOR)
    list(GET _components 2 Z3_VERSION_PATCH)
    list(GET _components 3 Z3_VERSION_TWEAK)
  else()
    message(WARNING "Failed to find z3_version.h; cannot extract Z3 version!")
    set(Z3_VERSION_MAJOR 0)
    set(Z3_VERSION_MINOR 0)
    set(Z3_VERSION_PATCH 0)
    set(Z3_VERSION_TWEAK 0)
    set(Z3_VERSION_STRING "${Z3_VERISON_MAJOR}.${Z3_VERSION_MINOR}.${Z3_VERSION_PATCH}.${Z3_VERSION_TWEAK}")
  endif()

  # Validate that both the public headers & the library files were found
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Z3 REQUIRED_VARS Z3_INCLUDE_DIR Z3_LIBRARY_DIR VERSION_VAR Z3_VERSION_STRING)

  # Create an imported target if all required variables were found
  if(Z3_FOUND)
    add_library(z3::libz3 UNKNOWN IMPORTED)
    set_target_properties(z3::libz3 PROPERTIES IMPORTED_LOCATION "${Z3_LIBRARY_DIR}")
    set_target_properties(z3::libz3 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${Z3_INCLUDE_DIR}")
    message(STATUS "Manually found Z3 (libs: ${Z3_LIBRARY_DIR}; public headers: ${Z3_INCLUDE_DIR})")

    # Expose other equivalents for variables exported by the regular Z3 CMake package
    set(Z3_CXX_INCLUDE_DIRS ${Z3_INCLUDE_DIR})
    set(Z3_C_INCLUDE_DIRS ${Z3_INCLUDE_DIR})
    set(Z3_LIBRARIES z3::libz3)
  elseif(Z3_FIND_REQUIRED)
    message(FATAL_ERROR "Failed to find Z3 library files/public headers!")
  elseif(NOT Z3_FIND_QUIETLY)
    message(WARNING "Failed to manually find Z3 libraries/headers")
  endif()
endif()
