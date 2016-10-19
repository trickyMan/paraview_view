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
#include "vtkSurfaceLICInterface.h"

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
  this->SurfaceLICMapper->SetCompositeDataDisplayAttributes(compositeAttributes);
  this->StreamLinesMapper->SetCompositeDataDisplayAttributes(compositeAttributes);
  compositeAttributes->Delete();

  // This will add the new mappers to the pipeline.
  this->SetupDefaults();
}

//----------------------------------------------------------------------------
vtkStreamLinesRepresentation::~vtkStreamLinesRepresentation()
{
#ifndef VTKGL2
  this->Painter->Delete();
  this->LODPainter->Delete();
#endif
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SetUseLICForLOD(bool val)
{
  this->UseLICForLOD = val;
#ifndef VTKGL2
  this->LODPainter->SetEnable(this->Painter->GetEnable() && this->UseLICForLOD);
#else
  this->SurfaceLICLODMapper->GetLICInterface()->SetEnable(
    (this->SurfaceLICMapper->GetLICInterface()->GetEnable() && this->UseLICForLOD)? 1 : 0);
#endif
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

  // the Surface LIC painter will make use of
  // MPI global collective comunication calls
  // need to disable IceT's empty image
  // optimization
  if (request_type == vtkPVView::REQUEST_UPDATE())
    {
    outInfo->Set(vtkPVRenderView::RENDER_EMPTY_IMAGES(), 1);
    }

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
#ifndef VTKGL2
  this->Painter->SetEnable(val);
  this->LODPainter->SetEnable(this->Painter->GetEnable() && this->UseLICForLOD);
#else
  this->SurfaceLICMapper->GetLICInterface()->SetEnable(val? 1 : 0);
  this->SurfaceLICLODMapper->GetLICInterface()->SetEnable((val && this->UseLICForLOD)? 1: 0);
#endif
}

// These are some settings that would help lod painter run faster.
// If the user really cares about speed then best to use a wireframe
// durring interaction
#if defined(vtkStreamLinesRepresentationFASTLOD)
//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SetStepSize(double val)
{

  // when interacting take half the number of steps at twice the
  // step size.
  double twiceVal=val*2.0;

#ifndef VTKGL2
  this->Painter->SetStepSize(val);
  this->LODPainter->SetStepSize(twiceVal);
#else
  this->SurfaceLICMapper->GetLICInterface()->SetStepSize(val);
  this->SurfaceLICLODMapper->GetLICInterface()->SetStepSize(twiceVal);
#endif
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SetNumberOfSteps(int val)
{
#ifndef VTKGL2
  this->Painter->SetNumberOfSteps(val);
#else
  this->SurfaceLICMapper->SetNumberOfSteps(val);
#endif

  // when interacting take half the number of steps at twice the
  // step size.
  int halfVal=val/2;
  if (halfVal<1) { halfVal=1; }
#ifndef VTKGL2
  this->LODPainter->GetLICInterface()->SetNumberOfSteps(halfVal);
#else
  this->SurfaceLICLODMapper->GetLICInterface()->SetNumberOfSteps(halfVal);
#endif
}

//----------------------------------------------------------------------------
#define vtkStreamLinesRepresentationPassParameterMacro(_name, _type)          \
void vtkStreamLinesRepresentation::Set##_name (_type val)                     \
{                                                                            \
#ifndef VTKGL2                                                               \
  this->Painter->Set##_name (val);                                           \
#else                                                                        \
  this->SurfaceLICMapper->GetLICInterface()->Set##_name (val);                                  \
#endif                                                                       \
}
vtkStreamLinesRepresentationPassParameterMacro( EnhancedLIC, int)
vtkStreamLinesRepresentationPassParameterMacro( EnhanceContrast, int)
vtkStreamLinesRepresentationPassParameterMacro( LowLICContrastEnhancementFactor, double)
vtkStreamLinesRepresentationPassParameterMacro( HighLICContrastEnhancementFactor, double)
vtkStreamLinesRepresentationPassParameterMacro( LowColorContrastEnhancementFactor, double)
vtkStreamLinesRepresentationPassParameterMacro( HighColorContrastEnhancementFactor, double)
vtkStreamLinesRepresentationPassParameterMacro( AntiAlias, int)
#endif

//----------------------------------------------------------------------------
#ifndef VTKGL2
#define vtkStreamLinesRepresentationPassParameterWithLODMacro(_name, _type)   \
void vtkStreamLinesRepresentation::Set##_name (_type val)                     \
{                                                                            \
  this->Painter->Set##_name (val);                                           \
  this->LODPainter->Set##_name (val);                                        \
}
#else
#define vtkStreamLinesRepresentationPassParameterWithLODMacro(_name, _type)   \
void vtkStreamLinesRepresentation::Set##_name (_type val)                     \
{                                                                            \
  this->SurfaceLICMapper->GetLICInterface()->Set##_name (val);                                  \
  this->SurfaceLICLODMapper->GetLICInterface()->Set##_name (val);                               \
}
#endif

#if !defined(vtkStreamLinesRepresentationFASTLOD)
vtkStreamLinesRepresentationPassParameterWithLODMacro( StepSize, double)
vtkStreamLinesRepresentationPassParameterWithLODMacro( NumberOfSteps, int)
vtkStreamLinesRepresentationPassParameterWithLODMacro( EnhancedLIC, int)
vtkStreamLinesRepresentationPassParameterWithLODMacro( EnhanceContrast, int)
vtkStreamLinesRepresentationPassParameterWithLODMacro( LowLICContrastEnhancementFactor, double)
vtkStreamLinesRepresentationPassParameterWithLODMacro( HighLICContrastEnhancementFactor, double)
vtkStreamLinesRepresentationPassParameterWithLODMacro( LowColorContrastEnhancementFactor, double)
vtkStreamLinesRepresentationPassParameterWithLODMacro( HighColorContrastEnhancementFactor, double)
vtkStreamLinesRepresentationPassParameterWithLODMacro( AntiAlias, int)
#endif
vtkStreamLinesRepresentationPassParameterWithLODMacro( NormalizeVectors, int)
vtkStreamLinesRepresentationPassParameterWithLODMacro( ColorMode, int)
vtkStreamLinesRepresentationPassParameterWithLODMacro( MapModeBias, double)
vtkStreamLinesRepresentationPassParameterWithLODMacro( LICIntensity, double)
vtkStreamLinesRepresentationPassParameterWithLODMacro( MaskOnSurface, int)
vtkStreamLinesRepresentationPassParameterWithLODMacro( MaskThreshold, double)
vtkStreamLinesRepresentationPassParameterWithLODMacro( MaskColor, double*)
vtkStreamLinesRepresentationPassParameterWithLODMacro( MaskIntensity, double)
vtkStreamLinesRepresentationPassParameterWithLODMacro( GenerateNoiseTexture, int)
vtkStreamLinesRepresentationPassParameterWithLODMacro( NoiseType, int)
vtkStreamLinesRepresentationPassParameterWithLODMacro( NoiseTextureSize, int)
vtkStreamLinesRepresentationPassParameterWithLODMacro( MinNoiseValue, double)
vtkStreamLinesRepresentationPassParameterWithLODMacro( MaxNoiseValue, double)
vtkStreamLinesRepresentationPassParameterWithLODMacro( NoiseGrainSize, int)
vtkStreamLinesRepresentationPassParameterWithLODMacro( NumberOfNoiseLevels, int)
vtkStreamLinesRepresentationPassParameterWithLODMacro( ImpulseNoiseProbability, double)
vtkStreamLinesRepresentationPassParameterWithLODMacro( ImpulseNoiseBackgroundValue, double)
vtkStreamLinesRepresentationPassParameterWithLODMacro( NoiseGeneratorSeed, int)
vtkStreamLinesRepresentationPassParameterWithLODMacro( CompositeStrategy, int)

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::SelectInputVectors(int a, int b, int c,
  int attributeMode, const char* name)
{
#ifndef VTKGL2
  (void) a;
  (void) b;
  (void) c;
  this->Painter->SetInputArrayToProcess(attributeMode, name);
  this->LODPainter->SetInputArrayToProcess(attributeMode, name);
#else
  this->SurfaceLICMapper->SetInputArrayToProcess(a, b, c, attributeMode, name);
  this->SurfaceLICLODMapper->SetInputArrayToProcess(a, b, c, attributeMode, name);
#endif
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::WriteTimerLog(const char *fileName)
{
  #if !defined(vtkSurfaceLICPainterTIME) && !defined(vtkLineIntegralConvolution2DTIME)
  (void)fileName;
  #else
    #ifndef VTKGL2
  this->Painter->WriteTimerLog(fileName);
    #else
  this->SurfaceLICMapper->WriteTimerLog(fileName);
    #endif
  #endif
}

//----------------------------------------------------------------------------
void vtkStreamLinesRepresentation::UpdateColoringParameters()
{
  this->Superclass::UpdateColoringParameters(); // check if this is still relevant.
  // never interpolate scalars for surface LIC
  // because geometry shader is not expecting it.
  this->Superclass::SetInterpolateScalarsBeforeMapping(0);
}
