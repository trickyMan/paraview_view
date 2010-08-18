## Thanks to John Bidiscombe of
## CSCS - Swiss National Supercomputing Centre for creating and contributing
## this plugin.
  
IF (QT_USE_FILE)
  INCLUDE(${QT_USE_FILE})
ENDIF (QT_USE_FILE)

INCLUDE_DIRECTORIES(${HDF5_INCLUDE_DIR}
  ${VTK_INCLUDE_DIR}
  ${PARAVIEW_INCLUDE_DIRS}
  ${PARAVIEW_GUI_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/H5Part/src
  )

ADD_DEFINITIONS(-DH5_USE_16_API)

ADD_PARAVIEW_PLUGIN(H5PartReader "1.0"
  SERVER_MANAGER_SOURCES
    vtkH5PartReader.cxx

  SERVER_SOURCES
    H5Part/src/H5Part.c
    H5Part/src/H5Block.c

  SERVER_MANAGER_XML
    H5PartServerManager.xml

  GUI_RESOURCE_FILES
    H5PartGUI.xml
)

SET_SOURCE_FILES_PROPERTIES(
  ${CMAKE_CURRENT_SOURCE_DIR}/H5Part/src/H5Part.c
  ${CMAKE_CURRENT_SOURCE_DIR}/H5Part/src/H5Block.c
  PROPERTIES LANGUAGE CXX
)