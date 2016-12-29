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
#include "vtkStreamLinesRepresentation.h"

#include "vtkCompositeDataDisplayAttributes.h"
#include "vtkInformation.h"
#include "vtkInformationRequestKey.h"
#include "vtkObjectFactory.h"
#include "vtkPVRenderView.h"

#include "vtkStreamLinesMapper.h"

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkStreamLinesRepresentation);

//----------------------------------------------------------------------------
vtkStreamLinesRepresentation::vtkStreamLinesRepresentation()
{
  this->Mapper->Delete();
  this->LODMapper->Delete();

  this->StreamLinesMapper = vtkStreamLinesMapper::New();
  this->Mapper = this->StreamLinesMapper;
  this->LODMapper = this->StreamLinesMapper;

  // setup composite display attributes
  vtkCompositeDataDisplayAttributes *compositeAttributes =
    vtkCompositeDataDisplayAttributes::New();
  this->StreamLinesMapper->SetCompositeDataDisplayAttributes(compositeAttributes);
  compositeAttributes->Delete();

  // This will add the new mappers to the pipeline.
  this->SetupDefaults();
}

//----------------------------------------------------------------------------
vtkStreamLinesRepresentation::~vtkStreamLinesRepresentation()
{
}

//----------------------------------------------------------------------------
int vtkStreamLinesRepresentation::ProcessViewRequest(
      vtkInformationRequestKey* request_type,
      vtkInformation* inInfo,
      vtkInformation* outInfo)
{
  if (!this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo))
    {
    // i.e. this->GetVisibility() == false, hence nothing to do.
    return 0;
    }

  // the painter will make use of
  // MPI global collective comunication calls
  // need to disable IceT's empty image
  // optimization
  if (request_type == vtkPVView::REQUEST_UPDATE())
    {
    outInfo->Set(vtkPVRenderView::RENDER_EMPTY_IMAGES(), 1);
    }

  this->MarkModified(); // Should this go into ProcessViewRequest() ?

  return 1;
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SetEnable(bool val)
{
  this->StreamLinesMapper->SetEnable(val? 1 : 0);
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SetStepSize(double val)
{
  double twiceVal=val*2.0;
  this->StreamLinesMapper->SetStepSize(val);
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SetNumberOfSteps(int val)
{
  this->StreamLinesMapper->SetNumberOfSteps(val);
}


//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SelectInputVectors(int a, int b, int c,
  int attributeMode, const char* name)
{
  this->StreamLinesMapper->SetInputArrayToProcess(a, b, c, attributeMode, name);
}
