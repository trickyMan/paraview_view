if(VTK_RENDERING_BACKEND STREQUAL "OpenGL2")
  pv_plugin(StreamLinesRepresentation
    DESCRIPTION "Add stream lines visualization as a representation support"
    DEFAULT_ENABLED)
endif()
