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
#include "vtkOpenGLCamera.h"
#include "vtkOpenGLError.h"
#include "vtkOpenGLFramebufferObject.h"
#include "vtkOpenGLBufferObject.h"
#include "vtkOpenGLRenderer.h"
#include "vtkOpenGLRenderUtilities.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkOpenGLShaderCache.h"
#include "vtkOpenGLTexture.h"
#include "vtkOpenGLVertexArrayObject.h"
#include "vtkOpenGLVertexBufferObject.h"
#include "vtkPoints.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkProperty.h"
#include "vtkRenderWindow.h"
#include "vtkShader.h"
#include "vtkShaderProgram.h"
#include "vtkTextureObject.h"
#include "vtkTextureObjectVS.h"  // a pass through shader

#include "vtk_glew.h"

extern const char* vtkStreamLines_fs;
extern const char* vtkStreamLines_vs;

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
  this->CurrentTexture = 0;
  this->FrameTexture = 0;
  this->ShaderCache = 0;
  this->Program = 0;
  this->BlendingProgram = 0;
  this->TextureProgram = 0;

  this->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS,
    vtkDataSetAttributes::VECTORS);
}

//-----------------------------------------------------------------------------
vtkStreamLinesMapper::~vtkStreamLinesMapper()
{
}

//-----------------------------------------------------------------------------
void vtkStreamLinesMapper::InitParticle(
  vtkImageData *inData, vtkDataArray* speedField, Particle* p)
{
  double bounds[6];
  inData->GetBounds(bounds);
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
    vtkIdType pid = inData->FindPoint(p->pos);
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
      this->InitParticle(inData, speedField, &p);
    }
  }
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::DrawParticles(vtkRenderer *ren, vtkActor *actor)
{
  vtkRenderWindow* renWin = ren->GetRenderWindow();

  vtkNew<vtkPoints> points;
  points->SetDataTypeToFloat();
  points->Allocate(this->Particles.size() * 2);

  std::vector<unsigned int> indices(this->Particles.size() * 2);

  // Build VAO
  for (size_t i = 0, cnt = 0; i < this->Particles.size(); ++i)
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
  vtkOpenGLCheckErrorMacro("failed after shader");

  // Create the VBO
  vtkNew<vtkOpenGLVertexBufferObject> vbo;
  vbo->CreateVBO(points.Get(), nbPoints * 2, 0, 0, 0, 0);
  vbo->Bind();
  vtkOpenGLCheckErrorMacro("failed after vbo");

  vtkNew<vtkOpenGLBufferObject> ibo;
  ibo->SetType(vtkOpenGLBufferObject::ElementArrayBuffer);
  ibo->Bind();
  ibo->Upload(&indices[0], nbPoints * 2, vtkOpenGLBufferObject::ElementArrayBuffer);
  vtkOpenGLCheckErrorMacro("failed after ibo");

  vtkNew<vtkOpenGLVertexArrayObject> vao;
  vao->Bind();
  vao->AddAttributeArray(this->Program, vbo.Get(),
    "vertexMC", vbo->VertexOffset, vbo->Stride, VTK_FLOAT, 3, false);
  vtkOpenGLCheckErrorMacro("failed after vao");

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);
  //glEnable(GL_DEPTH_TEST);

  // Perform rendering
  glLineWidth(2);//actor->GetProperty()->GetLineWidth());
  glDrawArrays(GL_LINES, 0, points->GetNumberOfPoints());

  vtkOpenGLCheckErrorMacro("failed after Render");

  ibo->Release();
  vao->Release();
  vbo->Release();

  this->CurrentBuffer->UnBind();
  this->CurrentBuffer->RestorePreviousBindingsAndBuffers();

  static float quadTCoords[8] = { 0., 0., 1., 0., 1., 1., 0., 1. };
  static float quadVerts[12] = { -1., -1., 0,  1., -1., 0.,  1., 1., 0.,  -1., 1., 0. };

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
  this->BlendingProgram->SetUniformf("alpha", this->Alpha);
  this->BlendingProgram->SetUniformi("prev",
    this->FrameTexture->GetTextureUnit());
  this->BlendingProgram->SetUniformi("current",
    this->CurrentTexture->GetTextureUnit());
  //glDisable(GL_DEPTH_TEST);
  //glEnable(GL_BLEND);
  //glBlendFunc(GL_ONE, GL_ONE);
  vtkOpenGLRenderUtilities::RenderQuad(
    quadVerts, quadTCoords, this->BlendingProgram, vaotb.Get());
  //glDisable(GL_BLEND);
  this->CurrentTexture->Deactivate();
  vaotb->Release();

  this->FrameBuffer->UnBind();
  this->FrameBuffer->RestorePreviousBindingsAndBuffers();
  //glEnable(GL_DEPTH_TEST);

  // Finally draw the framebuffer FBO onto the screen
  this->ShaderCache->ReadyShaderProgram(this->TextureProgram);
  vtkNew<vtkOpenGLVertexArrayObject> vaot;
  vaot->Bind();
  this->FrameTexture->Activate();
  this->TextureProgram->SetUniformi("source",
    this->FrameTexture->GetTextureUnit());
  //vtkNew<vtkOpenGLVertexArrayObject> vaot;
  //vaot->Bind();
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA);
  vtkOpenGLRenderUtilities::RenderQuad(
    quadVerts, quadTCoords, this->TextureProgram, vaot.Get());
  glDisable(GL_BLEND);
  this->FrameTexture->Deactivate();

  vaot->Release();


  // Update the current buffer as alpha blending: t*newbuffer+(1-t)*oldbuffer
  // TODO(jpouderoux)
  //this->CurrentBuffer =  this->Alpha * buffer + (1- this->Alpha)*this->CurrentBuffer;
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
  this->Particles.resize(this->NumberOfParticles);

  // Move particles
  this->UpdateParticles(inData, inVectors, ren);

  // Draw updated particles in a buffer
  this->DrawParticles(ren, actor);
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

  vtkOpenGLRenderWindow *renWin =
    vtkOpenGLRenderWindow::SafeDownCast(ren->GetRenderWindow());
  const int* size = renWin->GetSize();

  if (!this->CurrentTexture ||
    this->CurrentTexture->GetWidth() != size[0] ||
    this->CurrentTexture->GetHeight() != size[1])
  {
    RELEASE_VTKGL_OBJECT(this->CurrentTexture);
    this->CurrentTexture = vtkTextureObject::New();
    this->CurrentTexture->SetContext(renWin);
    this->CurrentTexture->Create2D(size[0], size[1], 4, VTK_UNSIGNED_CHAR, false);
  }

  if (!this->FrameTexture ||
    this->FrameTexture->GetWidth() != size[0] ||
    this->FrameTexture->GetHeight() != size[1])
  {
    RELEASE_VTKGL_OBJECT(this->FrameTexture);
    this->FrameTexture = vtkTextureObject::New();
    this->FrameTexture->SetContext(renWin);
    this->FrameTexture->Create2D(size[0], size[1], 4, VTK_UNSIGNED_CHAR, false);
  }

  if (!this->ShaderCache)
  {
    this->ShaderCache = renWin->GetShaderCache();
    this->Program = vtkShaderProgram::New();
    this->Program->GetVertexShader()->SetSource(vtkStreamLines_vs);
    this->Program->GetFragmentShader()->SetSource(vtkStreamLines_fs);

    std::string VSSource = vtkTextureObjectVS;

    this->BlendingProgram = vtkShaderProgram::New();
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
      "  vec4 c = pc * alpha + cc;\n"//mix(cc, pc, alpha);\n"
      "  gl_FragData[0] = vec4(c.rgb, 1.);\n"
      "}\n";
    this->BlendingProgram->GetVertexShader()->SetSource(VSSource);
    this->BlendingProgram->GetFragmentShader()->SetSource(FSTSource);

    this->TextureProgram = vtkShaderProgram::New();
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
    this->TextureProgram->GetVertexShader()->SetSource(VSSource);
    this->TextureProgram->GetFragmentShader()->SetSource(FSSource);
  }
}

//----------------------------------------------------------------------------
void vtkStreamLinesMapper::ReleaseGraphicsResources(vtkWindow *renWin)
{
  RELEASE_VTKGL_OBJECT(this->CurrentBuffer);
  RELEASE_VTKGL_OBJECT(this->FrameBuffer);
  RELEASE_VTKGL_OBJECT(this->CurrentTexture);
  RELEASE_VTKGL_OBJECT(this->FrameTexture);
  //RELEASE_VTKGL_OBJECT(this->ShaderCache);
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
