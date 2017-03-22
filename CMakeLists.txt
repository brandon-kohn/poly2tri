CMAKE_MINIMUM_REQUIRED(VERSION 3.3.0)

# Build Properties
SET(CMAKE_INSTALL_INCDIR "include/poly2tri" CACHE PATH "Default relative include path.")
SET(CMAKE_INSTALL_LIBDIR "lib${LIB_SUFFIX}" CACHE PATH "Default relative lib path.")

# Targets
# NOTE: Only static libs supported as STL are part of the interface.
PROJECT(poly2tri)
SET(HEADERS poly2tri/common/shapes.h
            poly2tri/sweep/advancing_front.h
            poly2tri/sweep/cdt.h
            poly2tri/sweep/sweep.h
            poly2tri/sweep/sweep_context.h
            poly2tri/poly2tri.h)
SET(SOURCES poly2tri/common/shapes.cc
            poly2tri/sweep/advancing_front.cc
            poly2tri/sweep/cdt.cc
            poly2tri/sweep/sweep.cc
            poly2tri/sweep/sweep_context.cc)
ADD_LIBRARY(poly2tri STATIC ${SOURCES} ${HEADERS})
INCLUDE_DIRECTORIES(poly2tri poly2tri/common poly2tri/sweep)
            
# Deployment
INSTALL ( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/poly2tri/
          DESTINATION ${CMAKE_INSTALL_INCDIR}
          FILES_MATCHING PATTERN "*.h")
INSTALL (TARGETS poly2tri
         ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
         LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
