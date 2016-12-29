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

#include "vtkBoundingBox.h"
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
#include "vtkOpenGLRenderer.h"
#include "vtkFrameBufferObject.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"


struct Particle
{
  double x;
  double y;
  double z;
  int lastRenderedX;
  int lastRenderedY;
  int lastRenderedZ;
  int timeToDeath;
};

void vtkStreamLinesMapper::InitParticle(vtkImageData *volume, Particle & p)
{
    vtkMinimalStandardRandomSequence * rand = this->RandomNumberSequence.Get();
    while()
    {
      //sample a location
      p.lastRenderedX = p.x = rand->GetRangeValue(volume->GetMinXBound(),volume->GetMaxXBound());
      this->RandomNumberSequence->Next();
      p.lastRenderedY = p.y = rand->GetRangeValue(volume->GetMinYBound(),volume->GetMaxYBound());
      this->RandomNumberSequence->Next();
      p.lastRenderedZ = p.z = rand->GetRangeValue(volume->GetMinZBound(),volume->GetMaxZBound());
      this->RandomNumberSequence->Next();
      p.timeToDeath = rand->GetRangeValue(1, this->MaxTimeToDeath);

      // get speed at this location
      double speedVect[3] = volume->getTupple(p.x,p.y,p.z);
      double speedNorm = vtkMath::Norm(speedVect);

      // TODO(bjacquet)
      // if (volume->maxSpeedOverAllVolume==0) break;

      // Do not sample in no-speed areas
      if(speedNorm==0) continue;

      // We might want more particles in low speed areas
      //  by rejecting a high percentage of sampled particles in high speed area
      break;
    }
}

void vtkStreamLinesMapper::UpdateParticles(vtkImageData *volume, vtkRenderer *ren)
{
  double dt=this->StepLength;
  vtkCamera * cam = ren->GetActiveCamera();
  vtkBoundingBox bbox(volume->GetBounds());
  for(size_t i=0;i<particles.size();++i)
  {
    Particle & p = particles[i];
    p.timeToDeath--;
    if (p.timeToDeath>0){
      //move particle
      localSpeed = vol->getValue(p.x,p.y,p.z);
      p.x += dt * localSpeed.x;
      p.y += dt * localSpeed.y;
      p.z += dt * localSpeed.z;
      // Kill if out-of-volume
      if(! bbox.ContainsPoint(p.x ,p.y, p.z))
      {
        p.timeToDeath=0;
      }
      // Kill if out-of-frustrum
      if (! cam->IsInFrustrum(p.x ,p.y, p.z))
      {
        p.timeToDeath=0;
      }
    }
    //resample dead and outof-bounds particles
    if(p.timeToDeath<=0)
    {
      InitParticle(volume, p);
    }
  }
}

void vtkStreamLinesMapper::DrawParticles(vtkRenderer *ren, vtkFrameBufferObject *buffer)
{
  buffer.clear();
  vtkCamera * cam = ren->GetActiveCamera();
  for(size_t i=0; i<this->Particles.size(); ++i)
  {
    Particle & p = this->Particles[i];
    if (p.timeToDeath>0)
    {
      // Draw particle motion line
      // TODO(jpouderoux) use opengl to write 3D-line projections into buffer
      /*g.beginPath();
      g.moveTo(p.lastRenderedX, p.lastRenderedY, p.lastRenderedZ);
      g.lineTo(p.x, p.y, p.z);
      g.stroke();
      */
      // update lastRenderedXYZ
      p.lastRenderedX = p.x;
      p.lastRenderedY = p.y;
      p.lastRenderedX = p.z;
    }
  }
  // buffer.stroke() ?? //TODO(jpouderoux)
}

//----------------------------------------------------------------------------
vtkStreamLinesMapper::vtkStreamLinesMapper()
{
  this->Alpha = 0.5;
  this->NumberOfParticles= 100;
  this->MaxTimeToDeath= 40;
  this->Particles = std::vector<Particle>();
  this->RandomNumberSequence = vtkSmartPointer<vtkMinimalStandardRandomSequence>::New();
  // initialize the RandomNumberSequence
  this->RandomNumberSequence->SetSeed(1);
  this->CurrentBuffer = vtkFrameBufferObject2();
}

//----------------------------------------------------------------------------
vtkStreamLinesMapper::~vtkStreamLinesMapper()
{
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::Render(vtkRenderer *ren, vtkVolume *vol)
{
  vtkImageData * volume = vtkImageData::SafeDownCast(this->GetExecutive()->GetInputData(0, 0));
  // Resize particles when needed (added ones are dead-born, and updated next)
  this->particles.resize(this->NumberOfParticles);

  // Move particles
  UpdateParticles(volume, this->particles);

  // Draw updated particles in a buffer
  vtkFrameBufferObject buffer;
  drawParticles(this->particles, ren, buffer);

  // Update the current buffer as alpha blending: t*newbuffer+(1-t)*oldbuffer
  // TODO(jpouderoux)
  this->CurrentBuffer =  this->Alpha * buffer + (1- this->Alpha)*this->CurrentBuffer;

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
