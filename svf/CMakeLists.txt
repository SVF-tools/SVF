# Define the core library
add_library(SvfCore)

# Inherit compiler & linker options by publically linking against the interface library
target_link_libraries(SvfCore PUBLIC SvfFlags)

# Find MySQL C++ Connector
find_library(MYSQL_CPP_CONN mysqlcppconn
    PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
    DOC "MySQL C++ Connector library"
)

if(NOT MYSQL_CPP_CONN)
    message(FATAL_ERROR "MySQL C++ Connector library not found. Please install libmysqlcppconn-dev")
endif()

# Find MySQL include directories
find_path(MYSQL_CPP_CONN_INCLUDE_DIR cppconn/driver.h
    PATHS /usr/include/mysql-cppconn-8 /usr/include/mysql-cppconn /usr/include /usr/local/include
    DOC "MySQL C++ Connector include directory"
)

if(NOT MYSQL_CPP_CONN_INCLUDE_DIR)
    message(FATAL_ERROR "MySQL C++ Connector headers not found. Please install libmysqlcppconn-dev")
endif()

# Link MySQL C++ Connector to SvfCore
target_link_libraries(SvfCore PUBLIC ${MYSQL_CPP_CONN})
target_include_directories(SvfCore PRIVATE ${MYSQL_CPP_CONN_INCLUDE_DIR})

# Find PostgreSQL (libpq)
find_package(PostgreSQL REQUIRED)

# Link PostgreSQL client library and expose headers for sources like WPAPass.cpp
target_link_libraries(SvfCore PUBLIC ${PostgreSQL_LIBRARIES})
target_include_directories(SvfCore PRIVATE ${PostgreSQL_INCLUDE_DIRS})

# Find fmt (for -lfmt)
find_library(FMT_LIBRARY fmt
    PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
    DOC "fmt formatting library"
)

if(NOT FMT_LIBRARY)
    message(FATAL_ERROR "fmt library not found. Please install libfmt-dev")
endif()

# Link fmt to SvfCore
target_link_libraries(SvfCore PUBLIC ${FMT_LIBRARY})

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
