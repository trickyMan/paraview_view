/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkStreamLinesMapper.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkStreamLinesMapper.h"

#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkDataArray.h"
#include "vtkDataSet.h"
#include "vtkExecutive.h"
#include "vtkFloatArray.h"
#include "vtkInformation.h"
#include "vtkMath.h"
#include "vtkMatrix4x4.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGL.h"
#include "vtkOpenGLRenderer.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkOpenGLError.h"


//----------------------------------------------------------------------------
vtkStreamLinesMapper::vtkStreamLinesMapper()
{
  this->NumberOfSteps = 10;
  this->StepSize = 0.;
}

//----------------------------------------------------------------------------
vtkStreamLinesMapper::~vtkStreamLinesMapper()
{
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::Render(vtkRenderer *ren, vtkVolume *vol)
{
  // TODO :)
  std::cout << "Render called on streamline mapper" << std::endl;
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::ReleaseGraphicsResources(vtkWindow *)
{

}

//----------------------------------------------------------------------------
int vtkStreamLinesMapper::FillInputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkImageData");
  return 1;
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
