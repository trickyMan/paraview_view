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

#include "vtkAlgorithmOutput.h"
#include "vtkCellData.h"
#include "vtkColorTransferFunction.h"
#include "vtkCommand.h"
#include "vtkExtentTranslator.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMath.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkOutlineSource.h"
#include "vtkPExtentTranslator.h"
#include "vtkPVCacheKeeper.h"
#include "vtkPVLODActor.h"
#include "vtkPVRenderView.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkSmartPointer.h"
#include "vtkStreamLinesMapper.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkStructuredData.h"

#include <algorithm>
#include <map>
#include <string>

namespace
{
//----------------------------------------------------------------------------
void vtkGetNonGhostExtent(int* resultExtent, vtkImageData* dataSet)
{
  // this is really only meant for topologically structured grids
  dataSet->GetExtent(resultExtent);

  if (vtkUnsignedCharArray* ghostArray = vtkUnsignedCharArray::SafeDownCast(
        dataSet->GetCellData()->GetArray(vtkDataSetAttributes::GhostArrayName())))
  {
    // We have a ghost array. We need to iterate over the array to prune ghost
    // extents.

    int pntExtent[6];
    std::copy(resultExtent, resultExtent + 6, pntExtent);

    int validCellExtent[6];
    vtkStructuredData::GetCellExtentFromPointExtent(pntExtent, validCellExtent);

    // The start extent is the location of the first cell with ghost value 0.
    for (vtkIdType cc = 0, numTuples = ghostArray->GetNumberOfTuples(); cc < numTuples; ++cc)
    {
      if (ghostArray->GetValue(cc) == 0)
      {
        int ijk[3];
        vtkStructuredData::ComputeCellStructuredCoordsForExtent(cc, pntExtent, ijk);
        validCellExtent[0] = ijk[0];
        validCellExtent[2] = ijk[1];
        validCellExtent[4] = ijk[2];
        break;
      }
    }

    // The end extent is the  location of the last cell with ghost value 0.
    for (vtkIdType cc = (ghostArray->GetNumberOfTuples() - 1); cc >= 0; --cc)
    {
      if (ghostArray->GetValue(cc) == 0)
      {
        int ijk[3];
        vtkStructuredData::ComputeCellStructuredCoordsForExtent(cc, pntExtent, ijk);
        validCellExtent[1] = ijk[0];
        validCellExtent[3] = ijk[1];
        validCellExtent[5] = ijk[2];
        break;
      }
    }

    // convert cell-extents to pt extents.
    resultExtent[0] = validCellExtent[0];
    resultExtent[2] = validCellExtent[2];
    resultExtent[4] = validCellExtent[4];

    resultExtent[1] = std::min(validCellExtent[1] + 1, resultExtent[1]);
    resultExtent[3] = std::min(validCellExtent[3] + 1, resultExtent[3]);
    resultExtent[5] = std::min(validCellExtent[5] + 1, resultExtent[5]);
  }
}
}

vtkStandardNewMacro(vtkStreamLinesRepresentation);
//----------------------------------------------------------------------------
vtkStreamLinesRepresentation::vtkStreamLinesRepresentation()
{
  this->VolumeMapper = vtkStreamLinesMapper::New();
  this->Property = vtkProperty::New();

  this->Actor = vtkPVLODActor::New();
  this->Actor->SetProperty(this->Property);

  this->CacheKeeper = vtkPVCacheKeeper::New();

  this->OutlineSource = vtkOutlineSource::New();
  this->OutlineMapper = vtkPolyDataMapper::New();

  this->Cache = vtkImageData::New();

  this->CacheKeeper->SetInputData(this->Cache);
  this->Actor->SetLODMapper(this->OutlineMapper);

  vtkMath::UninitializeBounds(this->DataBounds);
  this->DataSize = 0;

  this->Origin[0] = this->Origin[1] = this->Origin[2] = 0;
  this->Spacing[0] = this->Spacing[1] = this->Spacing[2] = 0;
  this->WholeExtent[0] = this->WholeExtent[2] = this->WholeExtent[4] = 0;
  this->WholeExtent[1] = this->WholeExtent[3] = this->WholeExtent[5] = -1;
}

//----------------------------------------------------------------------------
vtkStreamLinesRepresentation::~vtkStreamLinesRepresentation()
{
  this->VolumeMapper->Delete();
  this->Property->Delete();
  this->Actor->Delete();
  this->OutlineSource->Delete();
  this->OutlineMapper->Delete();
  this->CacheKeeper->Delete();

  this->Cache->Delete();
}

//----------------------------------------------------------------------------
int vtkStreamLinesRepresentation::FillInputPortInformation(int, vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkImageData");
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  return 1;
}

//----------------------------------------------------------------------------
int vtkStreamLinesRepresentation::ProcessViewRequest(
  vtkInformationRequestKey* request_type, vtkInformation* inInfo, vtkInformation* outInfo)
{
  if (!this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo))
  {
    this->MarkModified();
    return 0;
  }
  if (request_type == vtkPVView::REQUEST_UPDATE())
  {
    vtkPVRenderView::SetPiece(
      inInfo, this, this->OutlineSource->GetOutputDataObject(0), this->DataSize);
    // BUG #14792.
    // We report this->DataSize explicitly since the data being "delivered" is
    // not the data that should be used to make rendering decisions based on
    // data size.
    outInfo->Set(vtkPVRenderView::NEED_ORDERED_COMPOSITING(), 1);

    vtkPVRenderView::SetGeometryBounds(inInfo, this->DataBounds);

    // Pass partitioning information to the render view.
    vtkPVRenderView::SetOrderedCompositingInformation(inInfo, this,
      this->PExtentTranslator.GetPointer(), this->WholeExtent, this->Origin, this->Spacing);

    vtkPVRenderView::SetRequiresDistributedRendering(inInfo, this, true);
  }
  else if (request_type == vtkPVView::REQUEST_UPDATE_LOD())
  {
    vtkPVRenderView::SetRequiresDistributedRenderingLOD(inInfo, this, true);
  }
  else if (request_type == vtkPVView::REQUEST_RENDER())
  {
    this->UpdateMapperParameters();

    cout << "req render" << endl;

    vtkAlgorithmOutput* producerPort = vtkPVRenderView::GetPieceProducer(inInfo, this);
    if (producerPort)
    {
      this->OutlineMapper->SetInputConnection(producerPort);
    }
  }

  this->MarkModified();

  return 1;
}

//----------------------------------------------------------------------------
int vtkStreamLinesRepresentation::RequestData(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkMath::UninitializeBounds(this->DataBounds);
  this->DataSize = 0;
  this->Origin[0] = this->Origin[1] = this->Origin[2] = 0;
  this->Spacing[0] = this->Spacing[1] = this->Spacing[2] = 0;
  this->WholeExtent[0] = this->WholeExtent[2] = this->WholeExtent[4] = 0;
  this->WholeExtent[1] = this->WholeExtent[3] = this->WholeExtent[5] = -1;

  // Pass caching information to the cache keeper.
  this->CacheKeeper->SetCachingEnabled(this->GetUseCache());
  this->CacheKeeper->SetCacheTime(this->GetCacheKey());

  if (inputVector[0]->GetNumberOfInformationObjects() == 1)
  {
    vtkImageData* input = vtkImageData::GetData(inputVector[0], 0);
    if (!this->GetUsingCacheForUpdate())
    {
      this->Cache->ShallowCopy(input);
      if (input->HasAnyGhostCells())
      {
        int ext[6];
        vtkGetNonGhostExtent(ext, this->Cache);
        // Yup, this will modify the "input", but that okay for now. Ultimately,
        // we will teach the volume mapper to handle ghost cells and this won't
        // be needed. Once that's done, we'll need to teach the KdTree
        // generation code to handle overlap in extents, however.
        this->Cache->Crop(ext);
      }
    }
    this->CacheKeeper->Update();

    this->Actor->SetEnableLOD(0);
    this->VolumeMapper->SetInputConnection(this->CacheKeeper->GetOutputPort());

    vtkImageData* output = vtkImageData::SafeDownCast(this->CacheKeeper->GetOutputDataObject(0));
    this->OutlineSource->SetBounds(output->GetBounds());
    this->OutlineSource->GetBounds(this->DataBounds);
    this->OutlineSource->Update();

    this->DataSize = output->GetActualMemorySize();

    // Collect information about volume that is needed for data redistribution
    // later.
    this->PExtentTranslator->GatherExtents(output);
    output->GetOrigin(this->Origin);
    output->GetSpacing(this->Spacing);
    vtkStreamingDemandDrivenPipeline::GetWholeExtent(
      inputVector[0]->GetInformationObject(0), this->WholeExtent);
  }
  else
  {
    // when no input is present, it implies that this processes is on a node
    // without the data input i.e. either client or render-server, in which case
    // we show only the outline.
    this->VolumeMapper->RemoveAllInputs();
    this->Actor->SetEnableLOD(1);
  }

  return this->Superclass::RequestData(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
bool vtkStreamLinesRepresentation::IsCached(double cache_key)
{
  return this->CacheKeeper->IsCached(cache_key);
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::MarkModified()
{
  if (!this->GetUseCache())
  {
    // Cleanup caches when not using cache.
    this->CacheKeeper->RemoveAllCaches();
  }
  this->Superclass::MarkModified();
}

//----------------------------------------------------------------------------
bool vtkStreamLinesRepresentation::AddToView(vtkView* view)
{
  // FIXME: Need generic view API to add props.
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
  {
    rview->GetRenderer()->AddActor(this->Actor);
    // Indicate that this is a prop to be rendered during hardware selection.
    //rview->RegisterPropForHardwareSelection(this, this->GetRenderedProp());
    return this->Superclass::AddToView(view);
  }
  return false;
}

//----------------------------------------------------------------------------
bool vtkStreamLinesRepresentation::RemoveFromView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
  {
    rview->GetRenderer()->RemoveActor(this->Actor);
    return this->Superclass::RemoveFromView(view);
  }
  return false;
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::UpdateMapperParameters()
{
  /*
  const char* colorArrayName = NULL;
  int fieldAssociation = vtkDataObject::FIELD_ASSOCIATION_POINTS;

  vtkInformation* info = this->GetInputArrayInformation(0);
  if (info && info->Has(vtkDataObject::FIELD_ASSOCIATION()) &&
    info->Has(vtkDataObject::FIELD_NAME()))
  {
    colorArrayName = info->Get(vtkDataObject::FIELD_NAME());
    fieldAssociation = info->Get(vtkDataObject::FIELD_ASSOCIATION());
  }

  //this->VolumeMapper->SelectScalarArray(colorArrayName);
  this->VolumeMapper->SetInputArrayToProcess(0, 0, 0,
    vtkDataObject::FIELD_ASSOCIATION_POINTS, colorArrayName);
*/
  this->Actor->SetMapper(this->VolumeMapper);

  // this is necessary since volume mappers don't like empty arrays.
  this->Actor->SetVisibility(1); //colorArrayName != NULL && colorArrayName[0] != 0);
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//***************************************************************************
// Forwarded to Actor.
//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SetVisibility(bool val)
{
  this->Superclass::SetVisibility(val);
  this->Actor->SetVisibility(val ? 1 : 0);
  this->MarkModified();
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SetAlpha(double val)
{
  this->VolumeMapper->SetAlpha(val);
  this->MarkModified();
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SetStepLength(double val)
{
  this->VolumeMapper->SetStepLength(val);
  this->MarkModified();
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SetNumberOfParticles(int val)
{
  this->VolumeMapper->SetNumberOfParticles(val);
  this->MarkModified();
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SetMaxTimeToDeath(int val)
{
  this->VolumeMapper->SetMaxTimeToDeath(val);
  this->MarkModified();
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SelectInputVectors(int a, int b, int c,
  int attributeMode, const char* name)
{
  this->VolumeMapper->SetInputArrayToProcess(a, b, c, attributeMode, name);
  this->MarkModified();
}
