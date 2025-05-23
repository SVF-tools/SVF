# Define the core library
add_library(SvfCore)

# Inherit compiler & linker options by publically linking against the interface library
target_link_libraries(SvfCore PUBLIC SvfFlags)

# Gather & set all of the core library's source files by globbing all .h and .cpp files (recursively)
file(GLOB_RECURSE SVF_CORE_HEADERS ${CMAKE_CURRENT_LIST_DIR}/include/*.h)
file(GLOB_RECURSE SVF_CORE_SOURCES ${CMAKE_CURRENT_LIST_DIR}/lib/*.cpp)
target_sources(
  SvfCore
  PUBLIC FILE_SET HEADERS
                  BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}/include
                  FILES ${SVF_CORE_HEADERS}
  PRIVATE ${SVF_CORE_SOURCES}
)

# Only expose the headers in the source tree to in-tree users of SVF
target_include_directories(SvfCore PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)

# Set the library & .so version of the core library
set_target_properties(SvfCore PROPERTIES VERSION ${SVF_VERSION} SOVERSION ${SVF_VERSION_MAJOR})

# Install the core library's files to the regular/configured install tree
install(
  TARGETS SvfCore
  EXPORT SVFTargets
  RUNTIME DESTINATION ${SVF_INSTALL_BINDIR}
  LIBRARY DESTINATION ${SVF_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${SVF_INSTALL_LIBDIR}
          FILE_SET HEADERS
          DESTINATION ${SVF_INSTALL_INCLUDEDIR}
)
