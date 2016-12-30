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
#include "vtkMapper.h"
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
  /*vtkCompositeDataDisplayAttributes *compositeAttributes =
    vtkCompositeDataDisplayAttributes::New();
  this->StreamLinesMapper->SetCompositeDataDisplayAttributes(compositeAttributes);
  compositeAttributes->Delete();*/

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

  this->MarkModified(); // Let's ensure we always require new rendering pass

  return 1;
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}


//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SetAlpha(double val)
{
  this->StreamLinesMapper->SetAlpha(val);
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SetStepLength(double val)
{
  this->StreamLinesMapper->SetStepLength(val);
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SetNumberOfParticles(int val)
{
  this->StreamLinesMapper->SetNumberOfParticles(val);
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SetMaxTimeToDeath(int val)
{
  this->StreamLinesMapper->SetMaxTimeToDeath(val);
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SetEnable(bool val)
{
  this->StreamLinesMapper->SetEnable(val);
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SelectInputVectors(int a, int b, int c,
  int attributeMode, const char* name)
{
  this->StreamLinesMapper->SetInputArrayToProcess(a, b, c, attributeMode, name);
}

//----------------------------------------------------------------------------
int vtkStreamLinesRepresentation::FillInputPortInformation(int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkImageData");
  //info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkCompositeDataSet");

  // Saying INPUT_IS_OPTIONAL() is essential, since representations don't have
  // any inputs on client-side (in client-server, client-render-server mode) and
  // render-server-side (in client-render-server mode).
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);

  return 1;
}
