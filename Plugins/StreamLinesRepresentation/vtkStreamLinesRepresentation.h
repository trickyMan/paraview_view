/*=========================================================================

  Program:   ParaView
  Module:    $RCSfile$

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkStreamLinesRepresentation
// .SECTION Description
// vtkStreamLinesRepresentation extends vtkGeometryRepresentation.

#ifndef vtkStreamLinesRepresentation_h
#define vtkStreamLinesRepresentation_h

#include "vtkGeometryRepresentation.h"

class vtkInformation;
class vtkInformationRequestKey;
class vtkStreamLinesMapper;

class VTK_EXPORT vtkStreamLinesRepresentation : public vtkGeometryRepresentation
{
public:
  static vtkStreamLinesRepresentation* New();
  vtkTypeMacro(vtkStreamLinesRepresentation, vtkGeometryRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // vtkAlgorithm::ProcessRequest() equivalent for rendering passes. This is
  // typically called by the vtkView to request meta-data from the
  // representations or ask them to perform certain tasks e.g.
  // PrepareForRendering.
  virtual int ProcessViewRequest(vtkInformationRequestKey* request_type,
    vtkInformation* inInfo, vtkInformation* outInfo);

  //***************************************************************************
  // Forwarded to vtkStreamLinesMapper
  void SetEnable(bool val);
  void SetNumberOfSteps(int val);
  void SetStepSize(double val);
  void SelectInputVectors(int, int, int, int attributeMode, const char* name);

protected:
  vtkStreamLinesRepresentation();
  ~vtkStreamLinesRepresentation();

  // Description:
  // Overridden method to set parameters on vtkProperty and vtkMapper.
  void UpdateColoringParameters();

  vtkStreamLinesMapper* StreamLinesMapper;

  bool UseLICForLOD;
private:
  vtkStreamLinesRepresentation(const vtkStreamLinesRepresentation&) VTK_DELETE_FUNCTION;
  void operator=(const vtkStreamLinesRepresentation&) VTK_DELETE_FUNCTION;

};

#endif
