include_directories(
  ${VTK_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  )

if("${VTK_RENDERING_BACKEND}" STREQUAL "OpenGL2")
  add_definitions(-DVTKGL2)
endif()

add_paraview_plugin(
  StreamLinesRepresentation "2.0"
  DOCUMENTATION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/doc"
  SERVER_MANAGER_XML vtkStreamLinesRepresentation.xml
  SERVER_MANAGER_SOURCES vtkStreamLinesRepresentation.cxx
  )

# because these tests can be a little flakey provide the
# option to disable them on problematic systems.
SET(STREAMLINESREPRESENTATION_PLUGIN_TESTING
  ON CACHE BOOL
  "Explicitly disable SurfaceLIC plugin ctests"
  )
mark_as_advanced(STREAMLINESREPRESENTATION_PLUGIN_TESTING)

if (BUILD_TESTING AND STREAMLINESREPRESENTATION_PLUGIN_TESTING AND PARAVIEW_ENABLE_PYTHON)
  add_subdirectory(Testing/Python)
endif()
