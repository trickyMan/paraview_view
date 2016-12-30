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

#include "vtkActor.h"
#include "vtkBoundingBox.h"
#include "vtkCamera.h"
#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkDataArray.h"
#include "vtkDataSet.h"
#include "vtkExecutive.h"
#include "vtkFloatArray.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkMath.h"
#include "vtkMatrix4x4.h"
#include "vtkMinimalStandardRandomSequence.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGLFramebufferObject.h"
#include "vtkOpenGLRenderer.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkOpenGLTexture.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkRenderWindow.h"
#include "vtkShader.h"
#include "vtkShaderProgram.h"
#include "vtkTextureObject.h"

// Out simple particle data structure
struct Particle
{
  double pos[3];
  double prevPos[3];
  int timeToDeath;
};


//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkStreamLinesMapper)

//-----------------------------------------------------------------------------
vtkStreamLinesMapper::vtkStreamLinesMapper()
{
  this->Enable = true;
  this->Alpha = 0.5;
  this->StepLength = 1.0;
  this->NumberOfParticles = 100;
  this->MaxTimeToDeath = 40;
  this->Particles = std::vector<Particle>();
  this->RandomNumberSequence = vtkSmartPointer<vtkMinimalStandardRandomSequence>::New();
  // initialize the RandomNumberSequence
  this->RandomNumberSequence->SetSeed(1);
  this->CurrentBuffer = 0;
  this->FrameBuffer = 0;

  this->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS,
    vtkDataSetAttributes::VECTORS);
}

//-----------------------------------------------------------------------------
vtkStreamLinesMapper::~vtkStreamLinesMapper()
{
}

//-----------------------------------------------------------------------------
void vtkStreamLinesMapper::InitParticle(
  vtkImageData *volume, vtkDataArray* speedField, Particle* p)
{
  double bounds[6];
  volume->GetBounds(bounds);
  vtkMinimalStandardRandomSequence* rand = this->RandomNumberSequence.Get();
  bool added = false;
  do
  {
    //sample a location
    p->prevPos[0] = p->pos[0] = rand->GetRangeValue(bounds[0], bounds[1]);
    this->RandomNumberSequence->Next();
    p->prevPos[1] = p->pos[1] = rand->GetRangeValue(bounds[2], bounds[3]);
    this->RandomNumberSequence->Next();
    p->prevPos[2] = p->pos[2] = rand->GetRangeValue(bounds[4], bounds[5]);
    this->RandomNumberSequence->Next();
    p->timeToDeath = rand->GetRangeValue(1, this->MaxTimeToDeath);
    this->RandomNumberSequence->Next();

    // Check speed at this location
    double speedVec[3];
    vtkIdType pid = volume->FindPoint(p->pos);
    double speed = 0.;
    if (pid >= 0)
    {
      speedField->GetTuple(pid, speedVec);
      speed = vtkMath::Norm(speedVec);
      added = true;
    }

    // TODO(bjacquet)
    // if (volume->maxSpeedOverAllVolume==0) break;

    // Do not sample in no-speed areas
    if (speed == 0.)
    {
      added = false;
    }

    // We might want more particles in low speed areas
    //  by rejecting a high percentage of sampled particles in high speed area
  }
  while (!added);
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::UpdateParticles(
  vtkImageData *volume, vtkDataArray* speedField, vtkRenderer* ren)
{
  double dt = this->StepLength;
  vtkCamera* cam = ren->GetActiveCamera();
  vtkBoundingBox bbox(volume->GetBounds());
  for (size_t i = 0; i < this->Particles.size(); ++i)
  {
    Particle & p = this->Particles[i];
    p.timeToDeath--;
    if (p.timeToDeath > 0)
    {
      // Move the particle
      vtkIdType pid = volume->FindPoint(p.pos);
      if (pid > 0)
      {
        double localSpeed[3];
        speedField->GetTuple(pid, localSpeed);
        p.pos[0] += dt * localSpeed[0];
        p.pos[1] += dt * localSpeed[1];
        p.pos[2] += dt * localSpeed[2];
      }
      // Kill if out-of-volume or out-of-frustum
      if (pid < 0 || !bbox.ContainsPoint(p.pos))// || !cam->IsInFrustrum(p.pos))
      {
        p.timeToDeath = 0;
      }
    }
    if (p.timeToDeath <= 0)
    {
      // Resample dead or out-of-bounds particle
      this->InitParticle(volume, speedField, &p);
    }
  }
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::DrawParticles(vtkRenderer *ren)
{
  vtkRenderWindow* renWin = ren->GetRenderWindow();
  this->CurrentBuffer->SetContext(renWin);
  this->CurrentBuffer->SaveCurrentBindingsAndBuffers();
  this->CurrentBuffer->Bind();
  this->CurrentBuffer->AddColorAttachment(
    this->CurrentBuffer->GetBothMode(), 0, this->CurrentTexture);
  this->CurrentBuffer->AddDepthAttachment(); // auto create depth buffer
  this->CurrentBuffer->ActivateBuffer(0);
  this->CurrentBuffer->Start(
    this->CurrentTexture->GetWidth(), this->CurrentTexture->GetHeight());

  vtkCamera* cam = ren->GetActiveCamera();

  // Build VAO
  for (size_t i = 0; i < this->Particles.size(); ++i)
  {
    Particle& p = this->Particles[i];
    if (p.timeToDeath > 0)
    {
      // Draw particle motion line
      // TODO(jpouderoux) use opengl to write 3D-line projections into buffer
      /*g.beginPath();
      g.moveTo(p.prevPos);
      g.lineTo(p.pos);
      g.stroke();
      */
      // update lastRenderedXYZ
      memcpy(p.prevPos, p.pos, 3 * sizeof(double));
    }
  }
  // Perform rendering

  this->CurrentBuffer->UnBind();
  this->CurrentBuffer->RestorePreviousBindingsAndBuffers();
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::Render(vtkRenderer *ren, vtkActor *actor)
{
  if (!this->Enable)
  {
    return;
  }

  this->InitializeBuffers(ren);

  vtkImageData* volume =
    vtkImageData::SafeDownCast(this->GetExecutive()->GetInputData(0, 0));

  vtkDataArray* inVectors =
    this->GetInputArrayToProcess(0, 0, this->GetExecutive()->GetInputInformation());

  if (!inVectors ||inVectors->GetNumberOfComponents() != 3)
  {
    vtkDebugMacro(<<"No speed field vector to process!");
    return;
  }

  // Resize particles when needed (added ones are dead-born, and updated next)
  this->Particles.resize(this->NumberOfParticles);

  // Move particles
  this->UpdateParticles(volume, inVectors, ren);

  // Draw updated particles in a buffer
  this->DrawParticles(ren);

  // Update the current buffer as alpha blending: t*newbuffer+(1-t)*oldbuffer
  // TODO(jpouderoux)
  //this->CurrentBuffer =  this->Alpha * buffer + (1- this->Alpha)*this->CurrentBuffer;

  std::cout << "Render called on streamline mapper" << std::endl;
}

//----------------------------------------------------------------------------
#define RELEASE_VTKGL_OBJECT(_x) \
  if (_x) \
  { \
    _x->ReleaseGraphicsResources(renWin); \
    _x->Delete(); \
    _x = 0; \
  }

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::InitializeBuffers(vtkRenderer* ren)
{
  if (!this->CurrentBuffer)
  {
    this->CurrentBuffer = vtkOpenGLFramebufferObject::New();
    this->FrameBuffer = vtkOpenGLFramebufferObject::New();
  }

  vtkRenderWindow* renWin = ren->GetRenderWindow();
  const int* size = renWin->GetSize();

  if (!this->CurrentTexture ||
    this->CurrentTexture->GetWidth() != size[0] ||
    this->CurrentTexture->GetHeight() != size[1])
  {
    RELEASE_VTKGL_OBJECT(this->CurrentTexture);
    this->CurrentTexture = vtkTextureObject::New();
    this->CurrentTexture->SetContext(vtkOpenGLRenderWindow::SafeDownCast(renWin));
    this->CurrentTexture->Create2D(size[0], size[1], 4, VTK_FLOAT, false);
  }

  if (!this->FrameTexture ||
    this->FrameTexture->GetWidth() != size[0] ||
    this->FrameTexture->GetHeight() != size[1])
  {
    RELEASE_VTKGL_OBJECT(this->FrameTexture);
    this->FrameTexture = vtkTextureObject::New();
    this->FrameTexture->SetContext(vtkOpenGLRenderWindow::SafeDownCast(renWin));
    this->FrameTexture->Create2D(size[0], size[1], 4, VTK_FLOAT, false);
  }
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::ReleaseGraphicsResources(vtkWindow *renWin)
{
  RELEASE_VTKGL_OBJECT(this->CurrentBuffer);
  RELEASE_VTKGL_OBJECT(this->FrameBuffer);
  RELEASE_VTKGL_OBJECT(this->CurrentTexture);
  RELEASE_VTKGL_OBJECT(this->FrameTexture);
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
