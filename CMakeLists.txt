cmake_minimum_required(VERSION 3.0.2)

# Build Properties
set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE PATH "Default relative include path.")
set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "Default relative lib path.")

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
include_directories(poly2tri poly2tri/common poly2tri/sweep)
            
# Deployment
install ( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/poly2tri/
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/poly2tri
          FILES_MATCHING PATTERN "*.h")
install (TARGETS ${PROJECT_NAME}
         ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
         LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
