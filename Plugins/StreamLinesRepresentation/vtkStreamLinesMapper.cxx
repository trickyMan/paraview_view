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
#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkDataArray.h"
#include "vtkDataSet.h"
#include "vtkExecutive.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkMath.h"
#include "vtkMatrix4x4.h"
#include "vtkMinimalStandardRandomSequence.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGLBufferObject.h"
#include "vtkOpenGLCamera.h"
#include "vtkOpenGLError.h"
#include "vtkOpenGLFramebufferObject.h"
#include "vtkOpenGLRenderer.h"
#include "vtkOpenGLRenderUtilities.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkOpenGLShaderCache.h"
#include "vtkOpenGLTexture.h"
#include "vtkOpenGLVertexArrayObject.h"
#include "vtkOpenGLVertexBufferObject.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkPolyData.h"
#include "vtkProperty.h"
#include "vtkRenderWindow.h"
#include "vtkShader.h"
#include "vtkShaderProgram.h"
#include "vtkSmartPointer.h"
#include "vtkTextureObject.h"
#include "vtkTextureObjectVS.h"  // a pass through shader

#include "vtk_glew.h"

#include <vector>

extern const char* vtkStreamLinesBlending_fs;
extern const char* vtkStreamLinesCopy_fs;
extern const char* vtkStreamLines_fs;
extern const char* vtkStreamLines_vs;

//----------------------------------------------------------------------------
// Out simple particle data structure
struct Particle
{
  double pos[3];
  double prevPos[3];
  int timeToDeath;
};

//----------------------------------------------------------------------------
#define RELEASE_VTKGL_OBJECT(_x) \
  if (_x) \
  { \
    _x->ReleaseGraphicsResources(renWin); \
    _x->Delete(); \
    _x = 0; \
  }

//----------------------------------------------------------------------------
class vtkStreamLinesMapper::Private : public vtkObject
{
public:
  Private(vtkStreamLinesMapper* parent)
  {
    this->Parent = parent;
    this->RandomNumberSequence = vtkSmartPointer<vtkMinimalStandardRandomSequence>::New();
    // initialize the RandomNumberSequence
    this->RandomNumberSequence->SetSeed(1);
    this->ShaderCache = 0;
    this->CurrentBuffer = 0;
    this->FrameBuffer = 0;
    this->CurrentTexture = 0;
    this->FrameTexture = 0;
    this->Program = 0;
    this->BlendingProgram = 0;
    this->TextureProgram = 0;
    this->Particles = std::vector<Particle>();
  }

  void ReleaseGraphicsResources(vtkWindow *renWin)
  {
    RELEASE_VTKGL_OBJECT(this->CurrentBuffer);
    RELEASE_VTKGL_OBJECT(this->FrameBuffer);
    RELEASE_VTKGL_OBJECT(this->CurrentTexture);
    RELEASE_VTKGL_OBJECT(this->FrameTexture);
  }

  vtkStreamLinesMapper* Parent;
  vtkSmartPointer<vtkMinimalStandardRandomSequence> RandomNumberSequence;
  vtkOpenGLShaderCache* ShaderCache;
  vtkShaderProgram* Program;
  vtkShaderProgram* BlendingProgram;
  vtkShaderProgram* TextureProgram;
  vtkOpenGLFramebufferObject* CurrentBuffer;
  vtkOpenGLFramebufferObject* FrameBuffer;
  vtkTextureObject* CurrentTexture;
  vtkTextureObject* FrameTexture;

  vtkMTimeType CameraMTime;

  std::vector<Particle> Particles;

  void InitParticle(vtkImageData*, vtkDataArray*, int);
  void UpdateParticles(vtkImageData*, vtkDataArray*, vtkRenderer* ren);
  void DrawParticles(vtkRenderer* ren, vtkActor *actor);
  void InitializeBuffers(vtkRenderer* ren);
};

//-----------------------------------------------------------------------------
void vtkStreamLinesMapper::Private::InitParticle(
  vtkImageData *inData, vtkDataArray* speedField, int pid)
{
  Particle& p = this->Particles[pid];
  double bounds[6];
  inData->GetBounds(bounds);
  vtkMinimalStandardRandomSequence* rand = this->RandomNumberSequence.Get();
  bool added = false;
  do
  {
    //sample a location
    double x = rand->GetRangeValue(bounds[0], bounds[1]); rand->Next();
    double y = rand->GetRangeValue(bounds[2], bounds[3]); rand->Next();
    double z = rand->GetRangeValue(bounds[4], bounds[5]); rand->Next();
    p.prevPos[0] = p.pos[0] = x;
    p.prevPos[1] = p.pos[1] = y;
    p.prevPos[2] = p.pos[2] = z;
    p.timeToDeath = rand->GetRangeValue(1, this->Parent->MaxTimeToDeath); rand->Next();

    // Check speed at this location
    double speedVec[3];
    vtkIdType pid = inData->FindPoint(p.pos);
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
void vtkStreamLinesMapper::Private::UpdateParticles(
  vtkImageData *inData, vtkDataArray* speedField, vtkRenderer* ren)
{
  this->Particles.resize(this->Parent->NumberOfParticles);

  double dt = this->Parent->StepLength;
  vtkCamera* cam = ren->GetActiveCamera();
  vtkBoundingBox bbox(inData->GetBounds());
  for (size_t i = 0; i < this->Particles.size(); ++i)
  {
    Particle & p = this->Particles[i];
    p.timeToDeath--;
    if (p.timeToDeath > 0)
    {
      // Move the particle
      vtkIdType pid = inData->FindPoint(p.pos);
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
      this->InitParticle(inData, speedField, i);
    }
  }
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::Private::DrawParticles(vtkRenderer *ren, vtkActor *actor)
{
  this->InitializeBuffers(ren);

  vtkRenderWindow* renWin = ren->GetRenderWindow();

  vtkNew<vtkPoints> points;
  points->SetDataTypeToFloat();
  std::size_t nbParticles = this->Particles.size();
  points->Allocate(nbParticles * 2);

  std::vector<unsigned int> indices(nbParticles * 2);

  // Build VAO
  for (size_t i = 0, cnt = 0; i < nbParticles; ++i)
  {
    Particle& p = this->Particles[i];
    if (p.timeToDeath > 0)
    {
      // Draw particle motion line
      points->InsertNextPoint(p.prevPos);
      points->InsertNextPoint(p.pos);

      indices[cnt] = cnt; ++cnt;
      indices[cnt] = cnt; ++cnt;

      // update lastRenderedXYZ
      memcpy(p.prevPos, p.pos, 3 * sizeof(double));
    }
  }

  vtkIdType nbPoints = points->GetNumberOfPoints() / 2;

  const int* size = renWin->GetSize();
  vtkOpenGLCamera* cam = vtkOpenGLCamera::SafeDownCast(ren->GetActiveCamera());
  // [WMVD]C == {world, model, view, display} coordinates
  // E.g., WCDC == world to display coordinate transformation
  vtkMatrix4x4* wcdc;
  vtkMatrix4x4* wcvc;
  vtkMatrix3x3* norms;
  vtkMatrix4x4* vcdc;
  cam->GetKeyMatrices(ren, wcvc, norms, vcdc, wcdc);

  ////////////////////////////////////////////////
  // Pass 1: Render segment to current buffer FBO
  this->CurrentBuffer->SetContext(renWin);
  this->CurrentBuffer->SaveCurrentBindingsAndBuffers();
  this->CurrentBuffer->Bind();
  this->CurrentBuffer->AddColorAttachment(
    this->CurrentBuffer->GetBothMode(), 0, this->CurrentTexture);
  this->CurrentBuffer->AddDepthAttachment(); // auto create depth buffer
  this->CurrentBuffer->ActivateBuffer(0);
  this->CurrentBuffer->Start(
    this->CurrentTexture->GetWidth(), this->CurrentTexture->GetHeight());

  this->ShaderCache->ReadyShaderProgram(this->Program);
  if (this->Program->IsUniformUsed("MCDCMatrix") > 0)
    this->Program->SetUniformMatrix("MCDCMatrix", wcdc);
  double* col = actor->GetProperty()->GetDiffuseColor();
  float color[3];
  color[0] = static_cast<double>(col[0]);
  color[1] = static_cast<double>(col[1]);
  color[2] = static_cast<double>(col[2]);
  this->Program->SetUniform3f("color", color);

  // Create the VBO
  vtkNew<vtkOpenGLVertexBufferObject> vbo;
  vbo->CreateVBO(points.Get(), nbPoints * 2, 0, 0, 0, 0);
  vbo->Bind();

  vtkNew<vtkOpenGLBufferObject> ibo;
  ibo->SetType(vtkOpenGLBufferObject::ElementArrayBuffer);
  ibo->Bind();
  ibo->Upload(&indices[0], nbPoints * 2, vtkOpenGLBufferObject::ElementArrayBuffer);

  vtkNew<vtkOpenGLVertexArrayObject> vao;
  vao->Bind();
  vao->AddAttributeArray(this->Program, vbo.Get(),
    "vertexMC", vbo->VertexOffset, vbo->Stride, VTK_FLOAT, 3, false);

  // Perform rendering
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);
  glLineWidth(actor->GetProperty()->GetLineWidth());
  glDrawArrays(GL_LINES, 0, points->GetNumberOfPoints());
  vtkOpenGLCheckErrorMacro("Failed after rendering");

  ibo->Release();
  vao->Release();
  vbo->Release();

  this->CurrentBuffer->UnBind();
  this->CurrentBuffer->RestorePreviousBindingsAndBuffers();

  static float s_quadTCoords[8] =
  {
    0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f
  };
  static float s_quadVerts[12] =
  {
    -1.f, -1.f, 0.f,  1.f, -1.f, 0.f,  1.f, 1.f, 0.f,  -1.f, 1.f, 0.f
  };

  ////////////////////////////////////////////////////////////////////
  // Pass 2: Blend current and previous frame in the frame buffer FBO
  this->FrameBuffer->SetContext(renWin);
  this->FrameBuffer->SaveCurrentBindingsAndBuffers();
  this->FrameBuffer->Bind();
  this->FrameBuffer->AddColorAttachment(
    this->FrameBuffer->GetBothMode(), 0, this->FrameTexture);
  this->FrameBuffer->AddDepthAttachment(); // auto create depth buffer
  this->FrameBuffer->ActivateBuffer(0);
  this->FrameBuffer->Start(
    this->FrameTexture->GetWidth(), this->FrameTexture->GetHeight());

  if (this->CameraMTime < cam->GetMTime())
  {
    // Clear frame buffer if camera changed
    glClear(GL_COLOR_BUFFER_BIT);
    this->CameraMTime = cam->GetMTime();
  }

  this->ShaderCache->ReadyShaderProgram(this->BlendingProgram);
  vtkNew<vtkOpenGLVertexArrayObject> vaotb;
  vaotb->Bind();
  this->FrameTexture->Activate();
  this->CurrentTexture->Activate();
  this->BlendingProgram->SetUniformf("alpha", this->Parent->Alpha);
  this->BlendingProgram->SetUniformi("prev",
    this->FrameTexture->GetTextureUnit());
  this->BlendingProgram->SetUniformi("current",
    this->CurrentTexture->GetTextureUnit());
  vtkOpenGLRenderUtilities::RenderQuad(
    s_quadVerts, s_quadTCoords, this->BlendingProgram, vaotb.Get());
  this->CurrentTexture->Deactivate();
  vaotb->Release();

  this->FrameBuffer->UnBind();
  this->FrameBuffer->RestorePreviousBindingsAndBuffers();

  ////////////////////////////////////////////////////////////
  // Pass 3: Finally draw the framebuffer FBO onto the screen
  this->ShaderCache->ReadyShaderProgram(this->TextureProgram);
  vtkNew<vtkOpenGLVertexArrayObject> vaot;
  vaot->Bind();
  this->FrameTexture->Activate();
  this->TextureProgram->SetUniformi("source",
    this->FrameTexture->GetTextureUnit());
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  vtkOpenGLRenderUtilities::RenderQuad(
    s_quadVerts, s_quadTCoords, this->TextureProgram, vaot.Get());
  glDisable(GL_BLEND);
  this->FrameTexture->Deactivate();

  vaot->Release();
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::Private::InitializeBuffers(vtkRenderer* ren)
{
  if (!this->CurrentBuffer)
  {
    this->CurrentBuffer = vtkOpenGLFramebufferObject::New();
  }
  if (!this->FrameBuffer)
  {
    this->FrameBuffer = vtkOpenGLFramebufferObject::New();
  }

  vtkOpenGLRenderWindow *renWin =
    vtkOpenGLRenderWindow::SafeDownCast(ren->GetRenderWindow());
  const int* size = renWin->GetSize();

  if (!this->CurrentTexture)
  {
    this->CurrentTexture = vtkTextureObject::New();
    this->CurrentTexture->SetContext(renWin);
  }
  if (this->CurrentTexture->GetWidth() != size[0] ||
    this->CurrentTexture->GetHeight() != size[1])
  {
    this->CurrentTexture->Create2D(size[0], size[1], 4, VTK_UNSIGNED_CHAR, false);
  }

  if (!this->FrameTexture)
  {
    this->FrameTexture = vtkTextureObject::New();
    this->FrameTexture->SetContext(renWin);
  }

  if (this->FrameTexture->GetWidth() != size[0] ||
    this->FrameTexture->GetHeight() != size[1])
  {
    this->FrameTexture->Create2D(size[0], size[1], 4, VTK_UNSIGNED_CHAR, false);
  }

  if (!this->ShaderCache)
  {
    this->ShaderCache = renWin->GetShaderCache();
  }

  if (!this->Program)
  {
    this->Program =
      this->ShaderCache->ReadyShaderProgram(vtkStreamLines_vs, vtkStreamLines_fs, "");
  }

  if (!this->BlendingProgram)
  {
    this->BlendingProgram =
      this->ShaderCache->ReadyShaderProgram(vtkTextureObjectVS, vtkStreamLinesBlending_fs, "");
  }

  if (!this->TextureProgram)
  {
    this->TextureProgram =
      this->ShaderCache->ReadyShaderProgram(vtkTextureObjectVS, vtkStreamLinesCopy_fs, "");
  }
}

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkStreamLinesMapper)

//-----------------------------------------------------------------------------
vtkStreamLinesMapper::vtkStreamLinesMapper()
{
  this->Alpha = 0.95;
  this->StepLength = 0.01;
  this->NumberOfParticles = 1000;
  this->MaxTimeToDeath = 600;
  this->Internal = new Private(this);

  this->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS,
    vtkDataSetAttributes::VECTORS);
}

//-----------------------------------------------------------------------------
vtkStreamLinesMapper::~vtkStreamLinesMapper()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::Render(vtkRenderer *ren, vtkActor *actor)
{
  vtkImageData* inData =
    vtkImageData::SafeDownCast(this->GetInput());

  vtkDataArray* inVectors =
    this->GetInputArrayToProcess(0, 0, this->GetExecutive()->GetInputInformation());

  if (!inVectors ||inVectors->GetNumberOfComponents() != 3)
  {
    vtkDebugMacro(<<"No speed field vector to process!");
    return;
  }

  // Move particles
  this->Internal->UpdateParticles(inData, inVectors, ren);

  // Draw updated particles in a buffer
  this->Internal->DrawParticles(ren, actor);
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::ReleaseGraphicsResources(vtkWindow *renWin)
{
  this->Internal->ReleaseGraphicsResources(renWin);
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
