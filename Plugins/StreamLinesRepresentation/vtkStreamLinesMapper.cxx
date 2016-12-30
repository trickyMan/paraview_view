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
#include "vtkFloatArray.h"
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
class vtkStreamLinesMapper::Private
{
public:
  Private()
  {
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

  std::vector<Particle> Particles;

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
};

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkStreamLinesMapper)

//-----------------------------------------------------------------------------
vtkStreamLinesMapper::vtkStreamLinesMapper()
{
  this->Alpha = 0.95;
  this->StepLength = 0.01;
  this->NumberOfParticles = 1000;
  this->MaxTimeToDeath = 600;
  this->Internal = new Private();

  this->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS,
    vtkDataSetAttributes::VECTORS);
}

//-----------------------------------------------------------------------------
vtkStreamLinesMapper::~vtkStreamLinesMapper()
{
}

//-----------------------------------------------------------------------------
void vtkStreamLinesMapper::InitParticle(
  vtkImageData *inData, vtkDataArray* speedField, int pid)
{
  Particle& p = this->Internal->Particles[pid];
  double bounds[6];
  inData->GetBounds(bounds);
  vtkMinimalStandardRandomSequence* rand = this->Internal->RandomNumberSequence.Get();
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
    p.timeToDeath = rand->GetRangeValue(1, this->MaxTimeToDeath); rand->Next();

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
void vtkStreamLinesMapper::UpdateParticles(
  vtkImageData *inData, vtkDataArray* speedField, vtkRenderer* ren)
{
  double dt = this->StepLength;
  vtkCamera* cam = ren->GetActiveCamera();
  vtkBoundingBox bbox(inData->GetBounds());
  for (size_t i = 0; i < this->Internal->Particles.size(); ++i)
  {
    Particle & p = this->Internal->Particles[i];
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
void vtkStreamLinesMapper::DrawParticles(vtkRenderer *ren, vtkActor *actor)
{
  vtkRenderWindow* renWin = ren->GetRenderWindow();

  vtkNew<vtkPoints> points;
  points->SetDataTypeToFloat();
  std::size_t nbParticles = this->Internal->Particles.size();
  points->Allocate(nbParticles * 2);

  std::vector<unsigned int> indices(nbParticles * 2);

  // Build VAO
  for (size_t i = 0, cnt = 0; i < nbParticles; ++i)
  {
    Particle& p = this->Internal->Particles[i];
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
  this->Internal->CurrentBuffer->SetContext(renWin);
  this->Internal->CurrentBuffer->SaveCurrentBindingsAndBuffers();
  this->Internal->CurrentBuffer->Bind();
  this->Internal->CurrentBuffer->AddColorAttachment(
    this->Internal->CurrentBuffer->GetBothMode(), 0, this->Internal->CurrentTexture);
  this->Internal->CurrentBuffer->AddDepthAttachment(); // auto create depth buffer
  this->Internal->CurrentBuffer->ActivateBuffer(0);
  this->Internal->CurrentBuffer->Start(
    this->Internal->CurrentTexture->GetWidth(), this->Internal->CurrentTexture->GetHeight());

  this->Internal->ShaderCache->ReadyShaderProgram(this->Internal->Program);
  if (this->Internal->Program->IsUniformUsed("MCDCMatrix") > 0)
    this->Internal->Program->SetUniformMatrix("MCDCMatrix", wcdc);
  double* col = actor->GetProperty()->GetDiffuseColor();
  float color[3];
  color[0] = static_cast<double>(col[0]);
  color[1] = static_cast<double>(col[1]);
  color[2] = static_cast<double>(col[2]);
  this->Internal->Program->SetUniform3f("color", color);

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
  vao->AddAttributeArray(this->Internal->Program, vbo.Get(),
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

  this->Internal->CurrentBuffer->UnBind();
  this->Internal->CurrentBuffer->RestorePreviousBindingsAndBuffers();

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
  this->Internal->FrameBuffer->SetContext(renWin);
  this->Internal->FrameBuffer->SaveCurrentBindingsAndBuffers();
  this->Internal->FrameBuffer->Bind();
  this->Internal->FrameBuffer->AddColorAttachment(
    this->Internal->FrameBuffer->GetBothMode(), 0, this->Internal->FrameTexture);
  this->Internal->FrameBuffer->AddDepthAttachment(); // auto create depth buffer
  this->Internal->FrameBuffer->ActivateBuffer(0);
  this->Internal->FrameBuffer->Start(
    this->Internal->FrameTexture->GetWidth(), this->Internal->FrameTexture->GetHeight());

  if (this->Internal->CameraMTime < cam->GetMTime())
  {
    // Clear frame buffer if camera changed
    glClear(GL_COLOR_BUFFER_BIT);
    this->Internal->CameraMTime = cam->GetMTime();
  }

  this->Internal->ShaderCache->ReadyShaderProgram(this->Internal->BlendingProgram);
  vtkNew<vtkOpenGLVertexArrayObject> vaotb;
  vaotb->Bind();
  this->Internal->FrameTexture->Activate();
  this->Internal->CurrentTexture->Activate();
  this->Internal->BlendingProgram->SetUniformf("alpha", this->Alpha);
  this->Internal->BlendingProgram->SetUniformi("prev",
    this->Internal->FrameTexture->GetTextureUnit());
  this->Internal->BlendingProgram->SetUniformi("current",
    this->Internal->CurrentTexture->GetTextureUnit());
  vtkOpenGLRenderUtilities::RenderQuad(
    s_quadVerts, s_quadTCoords, this->Internal->BlendingProgram, vaotb.Get());
  this->Internal->CurrentTexture->Deactivate();
  vaotb->Release();

  this->Internal->FrameBuffer->UnBind();
  this->Internal->FrameBuffer->RestorePreviousBindingsAndBuffers();

  ////////////////////////////////////////////////////////////
  // Pass 3: Finally draw the framebuffer FBO onto the screen
  this->Internal->ShaderCache->ReadyShaderProgram(this->Internal->TextureProgram);
  vtkNew<vtkOpenGLVertexArrayObject> vaot;
  vaot->Bind();
  this->Internal->FrameTexture->Activate();
  this->Internal->TextureProgram->SetUniformi("source",
    this->Internal->FrameTexture->GetTextureUnit());
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  vtkOpenGLRenderUtilities::RenderQuad(
    s_quadVerts, s_quadTCoords, this->Internal->TextureProgram, vaot.Get());
  glDisable(GL_BLEND);
  this->Internal->FrameTexture->Deactivate();

  vaot->Release();
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::Render(vtkRenderer *ren, vtkActor *actor)
{
  this->InitializeBuffers(ren);

  vtkImageData* inData =
    vtkImageData::SafeDownCast(this->GetInput());

  vtkDataArray* inVectors =
    this->GetInputArrayToProcess(0, 0, this->GetExecutive()->GetInputInformation());

  if (!inVectors ||inVectors->GetNumberOfComponents() != 3)
  {
    vtkDebugMacro(<<"No speed field vector to process!");
    return;
  }

  // Resize particles when needed (added ones are dead-born, and updated next)
  this->Internal->Particles.resize(this->NumberOfParticles);

  // Move particles
  this->UpdateParticles(inData, inVectors, ren);

  // Draw updated particles in a buffer
  this->DrawParticles(ren, actor);
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::InitializeBuffers(vtkRenderer* ren)
{
  if (!this->Internal->CurrentBuffer)
  {
    this->Internal->CurrentBuffer = vtkOpenGLFramebufferObject::New();
  }
  if (!this->Internal->FrameBuffer)
  {
    this->Internal->FrameBuffer = vtkOpenGLFramebufferObject::New();
  }

  vtkOpenGLRenderWindow *renWin =
    vtkOpenGLRenderWindow::SafeDownCast(ren->GetRenderWindow());
  const int* size = renWin->GetSize();

  if (!this->Internal->CurrentTexture)
  {
    this->Internal->CurrentTexture = vtkTextureObject::New();
    this->Internal->CurrentTexture->SetContext(renWin);
  }
  if (this->Internal->CurrentTexture->GetWidth() != size[0] ||
    this->Internal->CurrentTexture->GetHeight() != size[1])
  {
    this->Internal->CurrentTexture->Create2D(size[0], size[1], 4, VTK_UNSIGNED_CHAR, false);
  }

  if (!this->Internal->FrameTexture)
  {
    this->Internal->FrameTexture = vtkTextureObject::New();
    this->Internal->FrameTexture->SetContext(renWin);
  }

  if (this->Internal->FrameTexture->GetWidth() != size[0] ||
    this->Internal->FrameTexture->GetHeight() != size[1])
  {
    this->Internal->FrameTexture->Create2D(size[0], size[1], 4, VTK_UNSIGNED_CHAR, false);
  }

  if (!this->Internal->ShaderCache)
  {
    this->Internal->ShaderCache = renWin->GetShaderCache();
  }

  if (!this->Internal->Program)
  {
    this->Internal->Program = vtkShaderProgram::New();
    this->Internal->Program->GetVertexShader()->SetSource(vtkStreamLines_vs);
    this->Internal->Program->GetFragmentShader()->SetSource(vtkStreamLines_fs);

    std::string VSSource = vtkTextureObjectVS;

    this->Internal->BlendingProgram = vtkShaderProgram::New();
        // build the shader source code
    std::string FSTSource =
      "//VTK::System::Dec\n"
      "//VTK::Output::Dec\n"
      "varying vec2 tcoordVC;\n"
      "uniform sampler2D prev;\n"
      "uniform sampler2D current;\n"
      "uniform float alpha;\n"
      "void main(void)\n"
      "{\n"
      "  vec4 pc = texture2D(prev, tcoordVC);\n"
      "  vec4 cc = texture2D(current, tcoordVC);\n"
      "  vec4 c = pc * alpha + cc;\n"
      "  gl_FragData[0] = vec4(c.rgb, 1.);\n"
      "}\n";
    this->Internal->BlendingProgram->GetVertexShader()->SetSource(VSSource);
    this->Internal->BlendingProgram->GetFragmentShader()->SetSource(FSTSource);

    this->Internal->TextureProgram = vtkShaderProgram::New();
        // build the shader source code
    std::string FSSource =
      "//VTK::System::Dec\n"
      "//VTK::Output::Dec\n"
      "varying vec2 tcoordVC;\n"
      "uniform sampler2D source;\n"
      "void main(void)\n"
      "{\n"
      "  gl_FragData[0] = texture2D(source, tcoordVC);\n"
      "}\n";
    this->Internal->TextureProgram->GetVertexShader()->SetSource(VSSource);
    this->Internal->TextureProgram->GetFragmentShader()->SetSource(FSSource);
  }
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
