include_directories(
  ${VTK_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  )

include(vtkOpenGL)

if("${VTK_RENDERING_BACKEND}" STREQUAL "OpenGL2")
  add_definitions(-DVTKGL2)
endif()

encode_files_as_strings(ENCODED_STRING_FILES
  shaders/vtkStreamLines_fs.glsl
  shaders/vtkStreamLines_vs.glsl
  )

add_paraview_plugin(
  StreamLinesRepresentation "0.1"
  DOCUMENTATION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/doc"
  SERVER_MANAGER_XML vtkStreamLinesRepresentation.xml
  SERVER_MANAGER_SOURCES
    vtkStreamLinesRepresentation.cxx
    vtkStreamLinesMapper.cxx
  SOURCES
    ${ENCODED_STRING_FILES}
  )

include(vtkOpenGL)
vtk_opengl_link(StreamLinesRepresentation)
