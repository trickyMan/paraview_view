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
#include "vtkDoubleArray.h"
#include "vtkExecutive.h"
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
#include "vtkScalarsToColors.h"
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
#define RELEASE_VTKGL_OBJECT(_x) \
  if (_x) \
  { \
    _x->ReleaseGraphicsResources(renWin); \
    _x->Delete(); \
    _x = 0; \
  }
#define RELEASE_VTKGL_OBJECT2(_x) \
  if (_x) \
  { \
    _x->ReleaseGraphicsResources(); \
    _x->Delete(); \
    _x = 0; \
  }

//----------------------------------------------------------------------------
class vtkStreamLinesMapper::Private : public vtkObject
{
public:
  static Private* New();

  void ReleaseGraphicsResources(vtkWindow *renWin)
  {
    RELEASE_VTKGL_OBJECT(this->BlendingProgram);
    RELEASE_VTKGL_OBJECT(this->CurrentBuffer);
    RELEASE_VTKGL_OBJECT(this->CurrentTexture);
    RELEASE_VTKGL_OBJECT(this->FrameBuffer);
    RELEASE_VTKGL_OBJECT(this->FrameTexture);
    RELEASE_VTKGL_OBJECT(this->Program);
    RELEASE_VTKGL_OBJECT(this->TextureProgram);
    RELEASE_VTKGL_OBJECT2(this->IndexBufferObject);
  }

  void SetMapper(vtkStreamLinesMapper* mapper)
  {
    this->Mapper = mapper;
  }

  vtkStreamLinesMapper* Mapper;
  vtkSmartPointer<vtkMinimalStandardRandomSequence> RandomNumberSequence;
  vtkOpenGLShaderCache* ShaderCache;
  vtkShaderProgram* Program;
  vtkShaderProgram* BlendingProgram;
  vtkShaderProgram* TextureProgram;
  vtkOpenGLFramebufferObject* CurrentBuffer;
  vtkOpenGLFramebufferObject* FrameBuffer;
  vtkTextureObject* CurrentTexture;
  vtkTextureObject* FrameTexture;
  vtkOpenGLBufferObject* IndexBufferObject;

  vtkMTimeType CameraMTime;

  //std::vector<Particle> Particles;
  vtkNew<vtkPoints> Particles;
  vtkNew<vtkDoubleArray> InterpolationArray;
  vtkSmartPointer<vtkDataArray> InterpolationScalarArray;
  vtkDataArray* PrevScalars;
  vtkNew<vtkIdList> IdList;
  std::vector<unsigned char> ParticleColors;
  std::vector<int> ParticlesTTL;
  std::vector<int> Indices;

  bool RebuildBufferObjects;

  void SetNumberOfParticles(int);
  void InitParticle(vtkDataSet*, vtkDataArray*, vtkDataArray*, int);
  void UpdateParticles(vtkDataSet*, vtkDataArray*, vtkDataArray*, vtkRenderer*);
  void DrawParticles(vtkRenderer*, vtkActor*);
  void InitializeBuffers(vtkRenderer*);
  bool InterpolateSpeedAndColor(
    vtkDataSet*, vtkDataArray*, vtkDataArray*, double[3], double[3], vtkIdType);

  double Rand(double vmin = 0., double vmax = 1.)
  {
    this->RandomNumberSequence->Next();
    return this->RandomNumberSequence->GetRangeValue(vmin, vmax);
  }

protected:
  Private()
  {
    this->Mapper = 0;
    this->RandomNumberSequence =
      vtkSmartPointer<vtkMinimalStandardRandomSequence>::New();
    this->RandomNumberSequence->SetSeed(1);
    this->ShaderCache = 0;
    this->CurrentBuffer = 0;
    this->FrameBuffer = 0;
    this->CurrentTexture = 0;
    this->FrameTexture = 0;
    this->Program = 0;
    this->BlendingProgram = 0;
    this->TextureProgram = 0;
    this->IndexBufferObject = 0;
    this->Particles->SetDataTypeToFloat();
    this->RebuildBufferObjects = true;
    this->InterpolationArray->SetNumberOfComponents(3);
    this->InterpolationArray->SetNumberOfTuples(1);
    this->PrevScalars = reinterpret_cast<vtkDataArray*>(0x01);
  }

  ~Private()
  {
  }

private:
  Private(const Private&) VTK_DELETE_FUNCTION;
  void operator=(const Private&) VTK_DELETE_FUNCTION;
};

vtkStandardNewMacro(vtkStreamLinesMapper::Private)

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::Private::SetNumberOfParticles(int nbParticles)
{
  this->Particles->Resize(nbParticles * 2);
  this->ParticlesTTL.resize(nbParticles, 0);
  this->ParticleColors.resize(nbParticles * 2 * 4);
  this->Indices.resize(nbParticles * 2);

  // Build indices array
  for (size_t i = 0; i < nbParticles * 2; i++)
  {
    this->Indices[i] = i;
  }

  this->RebuildBufferObjects = true;
}

//-----------------------------------------------------------------------------
bool vtkStreamLinesMapper::Private::InterpolateSpeedAndColor(
  vtkDataSet* inData, vtkDataArray* speedField, vtkDataArray* scalars, double pos[3],
  double outSpeed[3], vtkIdType pid)
{
  int subId;
  double pcoords[3];
  static double weights[1024];
  vtkIdType cellId =
    inData->FindCell(pos, 0, -1, 1e-10, subId, pcoords, weights);
  if (cellId < 0)
  {
    return false;
  }

  if (!speedField && !scalars)
  {
    return true;
  }

  inData->GetCellPoints(cellId, this->IdList.Get());
  if (speedField)
  {
    this->InterpolationArray->InterpolateTuple(0, this->IdList.Get(), speedField, weights);
    this->InterpolationArray->GetTuple(0, outSpeed);
  }

  if (scalars)
  {
    this->InterpolationScalarArray->InterpolateTuple(pid, this->IdList.Get(), scalars, weights);
  }
  return true;
}

//-----------------------------------------------------------------------------
void vtkStreamLinesMapper::Private::InitParticle(
  vtkDataSet *inData, vtkDataArray* speedField, vtkDataArray* scalars, int pid)
{
  //Particle& p = this->Particles[pid];
  double bounds[6];
  inData->GetBounds(bounds);
  bool added = false;
  do
  {
    // Sample a new seed location
    double pos[3];
    pos[0] = this->Rand(bounds[0], bounds[1]);
    pos[1] = this->Rand(bounds[2], bounds[3]);
    pos[2] = this->Rand(bounds[4], bounds[5]);
    this->Particles->SetPoint(pid * 2 + 0, pos);
    this->Particles->SetPoint(pid * 2 + 1, pos);
    this->ParticlesTTL[pid] = this->Rand(1, this->Mapper->MaxTimeToLive);

    // Check speed at this location
    double speedVec[9];
    double speed = 0.;
    if (this->InterpolateSpeedAndColor(inData, speedField, scalars, pos, speedVec, pid * 2))
    {
      this->InterpolationScalarArray->SetTuple(pid * 2 + 1, this->InterpolationScalarArray->GetTuple(pid * 2));
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
  vtkDataSet *inData, vtkDataArray* speedField, vtkDataArray* scalars, vtkRenderer* ren)
{
  std::size_t nbParticles = this->ParticlesTTL.size();

  if (this->PrevScalars != scalars)
  {
    if (scalars)
    {
      this->InterpolationScalarArray =
        vtkDataArray::CreateDataArray(scalars->GetDataType());
    }
    else
    {
      this->InterpolationScalarArray =
        vtkUnsignedCharArray::New();
    }
    this->InterpolationScalarArray->SetNumberOfComponents(
      scalars ? scalars->GetNumberOfComponents() : 1);
    this->InterpolationScalarArray->SetNumberOfTuples(nbParticles * 2);
    this->PrevScalars = scalars;
  }

  double dt = this->Mapper->StepLength;
  vtkCamera* cam = ren->GetActiveCamera();
  vtkBoundingBox bbox(inData->GetBounds());

  for (size_t i = 0; i < nbParticles; ++i)
  {
    this->ParticlesTTL[i]--;
    if (this->ParticlesTTL[i] > 0)
    {
      double pos[3];
      this->Particles->GetPoint(i * 2 + 1, pos);

      // Update prevpos with last pos
      this->Particles->SetPoint(i * 2 + 0, pos);
      this->InterpolationScalarArray->SetTuple(
        i * 2 + 0, this->InterpolationScalarArray->GetTuple(i * 2 + 1));

      // Move the particle and fetch its color
      double speedVec[3];
      if (this->InterpolateSpeedAndColor(
        inData, speedField, scalars, pos, speedVec, i * 2 + 1))
      {
        this->Particles->SetPoint(2 * i + 1,
          pos[0] + dt * speedVec[0],
          pos[1] + dt * speedVec[1],
          pos[2] + dt * speedVec[2]);
      }
      else
      {
        this->ParticlesTTL[i] = 0;
      }
      // Kill if particle is out-of-frustum
      /*if !cam->IsInFrustrum(pos))
      {
        this->ParticlesTTL[i] = 0;
      }*/
    }
    if (this->ParticlesTTL[i] <= 0)
    {
      // Resample dead or out-of-bounds particle
      this->InitParticle(inData, speedField, scalars, i);
    }
  }
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::Private::DrawParticles(vtkRenderer *ren, vtkActor *actor)
{
  this->InitializeBuffers(ren);

  vtkRenderWindow* renWin = ren->GetRenderWindow();

  std::size_t nbParticles = this->ParticlesTTL.size();

  const int* size = renWin->GetSize();
  vtkOpenGLCamera* cam = vtkOpenGLCamera::SafeDownCast(ren->GetActiveCamera());
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
  this->Program->SetUniformi("scalarVisibility", this->Mapper->GetScalarVisibility());

  // Setup the IBO
  this->IndexBufferObject->Bind();
  if (this->RebuildBufferObjects)
  {
    // We upload the indices only when number of particles changed
    this->IndexBufferObject->Upload(&this->Indices[0], nbParticles * 2,
      vtkOpenGLBufferObject::ElementArrayBuffer);
    this->RebuildBufferObjects = false;
  }

  vtkUnsignedCharArray* colors = this->Mapper->GetLookupTable()->MapScalars(
    this->InterpolationScalarArray, VTK_COLOR_MODE_DEFAULT, -1);

  // Create the VBO
  vtkNew<vtkOpenGLVertexBufferObject> vbo;
  vbo->CreateVBO(this->Particles.Get(), nbParticles * 2, 0, 0,
    colors->GetPointer(0), 4);
  vbo->Bind();
  colors->Delete();

  // Setup the VAO
  vtkNew<vtkOpenGLVertexArrayObject> vao;
  vao->Bind();
  vao->AddAttributeArray(this->Program, vbo.Get(),
    "vertexMC", vbo->VertexOffset, vbo->Stride, VTK_FLOAT, 3, false);
  vao->AddAttributeArray(this->Program, vbo.Get(),
    "scalarColor", vbo->ColorOffset, vbo->Stride, VTK_UNSIGNED_CHAR,
    vbo->ColorComponents, true);

  // Perform rendering
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);
  glLineWidth(actor->GetProperty()->GetLineWidth());
  glDrawArrays(GL_LINES, 0, nbParticles * 2);
  vtkOpenGLCheckErrorMacro("Failed after rendering");

  this->IndexBufferObject->Release();
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
  double alpha = 1. - 1./(this->Mapper->MaxTimeToLive * this->Mapper->Alpha);
  this->BlendingProgram->SetUniformf("alpha", alpha);
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
    this->CurrentTexture->Create2D(size[0], size[1], 4, VTK_FLOAT, false);
    this->CameraMTime = 0;
  }

  if (!this->FrameTexture)
  {
    this->FrameTexture = vtkTextureObject::New();
    this->FrameTexture->SetContext(renWin);
  }

  if (this->FrameTexture->GetWidth() != size[0] ||
    this->FrameTexture->GetHeight() != size[1])
  {
    this->FrameTexture->Create2D(size[0], size[1], 4, VTK_FLOAT, false);
    this->CameraMTime = 0;
  }

  if (!this->ShaderCache)
  {
    this->ShaderCache = renWin->GetShaderCache();
  }

  if (!this->Program)
  {
    this->Program = this->ShaderCache->ReadyShaderProgram(
      vtkStreamLines_vs, vtkStreamLines_fs, "");
    this->Program->Register(this);
  }

  if (!this->BlendingProgram)
  {
    this->BlendingProgram = this->ShaderCache->ReadyShaderProgram(
      vtkTextureObjectVS, vtkStreamLinesBlending_fs, "");
    this->BlendingProgram->Register(this);
  }

  if (!this->TextureProgram)
  {
    this->TextureProgram = this->ShaderCache->ReadyShaderProgram(
      vtkTextureObjectVS, vtkStreamLinesCopy_fs, "");
    this->TextureProgram->Register(this);
  }

  if (!this->IndexBufferObject)
  {
    this->IndexBufferObject = vtkOpenGLBufferObject::New();
    this->IndexBufferObject->SetType(vtkOpenGLBufferObject::ElementArrayBuffer);
  }
}

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkStreamLinesMapper)

//-----------------------------------------------------------------------------
vtkStreamLinesMapper::vtkStreamLinesMapper()
{
  this->Internal = Private::New();
  this->Internal->SetMapper(this);
  this->Alpha = 0.95;
  this->StepLength = 0.01;
  this->MaxTimeToLive = 600;
  this->NumberOfParticles = 0;
  this->SetNumberOfParticles(1000);

  this->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS,
    vtkDataSetAttributes::SCALARS);
  this->SetInputArrayToProcess(1, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS,
    vtkDataSetAttributes::VECTORS);
}

//-----------------------------------------------------------------------------
vtkStreamLinesMapper::~vtkStreamLinesMapper()
{
  this->Internal->Delete();
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::SetNumberOfParticles(int nbParticles)
{
  if (this->NumberOfParticles == nbParticles)
  {
    return;
  }
  this->NumberOfParticles = nbParticles;
  this->Internal->SetNumberOfParticles(nbParticles);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::Render(vtkRenderer *ren, vtkActor *actor)
{
  vtkDataSet* inData =
    vtkDataSet::SafeDownCast(this->GetInput());
  if (!inData || inData->GetNumberOfCells() == 0)
  {
    return;
  }

  vtkDataArray* inScalars =
    this->GetInputArrayToProcess(0, 0, this->GetExecutive()->GetInputInformation());

  vtkDataArray* inVectors =
    this->GetInputArrayToProcess(1, 0, this->GetExecutive()->GetInputInformation());

  if (!inVectors || inVectors->GetNumberOfComponents() != 3)
  {
    vtkDebugMacro(<<"No speed field vector to process!");
    return;
  }

  // Move particles
  this->Internal->UpdateParticles(inData, inVectors, inScalars, ren);

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
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  return 1;
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
