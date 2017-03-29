cmake_minimum_required(VERSION 3.0.2)

# Build Properties
set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE PATH "Default relative include path.")
set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "Default relative lib path.")

# Version 
# Find '#define POLY2TRI_VERSION "x.x.z"' and extract version
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/poly2tri/version.hpp VERSION_FILE)
string(REGEX MATCH "#define[\\t\\ ]+POLY2TRI_VERSION[\\t\\ ]+\"([0-9]+\\.[0-9]+\\.[0-9]+)\"" VERSION_STRING "${VERSION_FILE}")
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\1;\\2;\\3" VERSION_RESULT "${CMAKE_MATCH_1}")
list(GET VERSION_RESULT 0 VERSION_MAJOR)
list(GET VERSION_RESULT 1 VERSION_MINOR)
list(GET VERSION_RESULT 2 VERSION_PATCH)
set (POLY2TRI_VERSION_STRING ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
message(STATUS "Poly2tri version ${POLY2TRI_VERSION_STRING}.")

# Targets
# NOTE: Only static libs supported as STL are part of the interface.
project(poly2tri)
set(HEADERS poly2tri/common/shapes.h
            poly2tri/sweep/advancing_front.h
            poly2tri/sweep/cdt.h
            poly2tri/sweep/sweep.h
            poly2tri/sweep/sweep_context.h
            poly2tri/poly2tri.h)
set(SOURCES poly2tri/common/shapes.cc
            poly2tri/sweep/advancing_front.cc
            poly2tri/sweep/cdt.cc
            poly2tri/sweep/sweep.cc
            poly2tri/sweep/sweep_context.cc)
add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "d")
set_target_properties (${PROJECT_NAME} PROPERTIES VERSION ${POLY2TRI_VERSION_STRING})
target_include_directories(poly2tri INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/poly2tri>
  $<INSTALL_INTERFACE:include>
)
include_directories(poly2tri poly2tri/common poly2tri/sweep)
            
# Deployment
install ( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/poly2tri/
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/poly2tri
          FILES_MATCHING PATTERN "*.h")
install (TARGETS ${PROJECT_NAME}
         ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
         LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
