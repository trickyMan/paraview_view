/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkStreamLinesMapper.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkStreamLinesMapper
 * @brief   Mapper for live stream lines rendering of 3D image data
 *
 *
 * @sa
 * vtkVolumeMapper vtkUnstructuredGridVolumeMapper
*/

#ifndef vtkStreamLinesMapper_h
#define vtkStreamLinesMapper_h

#include "vtkMapper.h"
#include "vtkSmartPointer.h"

#include <vector> // For the particle array

class vtkActor;
class vtkDataSet;
class vtkImageData;
class vtkMinimalStandardRandomSequence;
class vtkOpenGLFramebufferObject;
class vtkRenderer;
class vtkTextureObject;
class vtkWindow;
struct Particle;

class VTK_EXPORT vtkStreamLinesMapper : public vtkMapper
{
public:
  static vtkStreamLinesMapper* New();
  vtkTypeMacro(vtkStreamLinesMapper, vtkMapper);
  void PrintSelf(ostream& os, vtkIndent indent);

  //@{
  /**
   * Get/Set the enable state.
   */
  vtkSetMacro(Enable, bool);
  vtkGetMacro(Enable, bool);
  //@}


  //@{
  /**
   * Get/Set the Alpha blending between new trajectory and previous.
   */
  vtkSetMacro(Alpha, double);
  vtkGetMacro(Alpha, double);
  //@}

  //@{
  /**
   * Get/Set the StepLength.
   */
  vtkSetMacro(StepLength, double);
  vtkGetMacro(StepLength, double);
  //@}

  //@{
  /**
   * Get/Set the NumberOfParticles.
   */
  vtkSetMacro(NumberOfParticles, int);
  vtkGetMacro(NumberOfParticles, int);
  //@}

  //@{
  /**
   * Get/Set the MaxTimeToDeath (number of iteration before killing a particle).
   */
  vtkSetMacro(MaxTimeToDeath, int);
  vtkGetMacro(MaxTimeToDeath, int);
  //@}


  /**
   * WARNING: INTERNAL METHOD - NOT INTENDED FOR GENERAL USE
   * DO NOT USE THIS METHOD OUTSIDE OF THE RENDERING PROCESS
   * Render the volume
   */
  virtual void Render(vtkRenderer *ren, vtkActor *vol);

  /**
   * WARNING: INTERNAL METHOD - NOT INTENDED FOR GENERAL USE
   * Release any graphics resources that are being consumed by this mapper.
   * The parameter window could be used to determine which graphic
   * resources to release.
   */
  virtual void ReleaseGraphicsResources(vtkWindow *);

protected:
  vtkStreamLinesMapper();
  ~vtkStreamLinesMapper();

  double Alpha;
  double StepLength;
  int MaxTimeToDeath;
  int NumberOfParticles;
  bool Enable;
  std::vector<Particle> Particles;
  vtkSmartPointer<vtkMinimalStandardRandomSequence> RandomNumberSequence;
  vtkOpenGLFramebufferObject* CurrentBuffer;
  vtkOpenGLFramebufferObject* FrameBuffer;
  vtkTextureObject* CurrentTexture;
  vtkTextureObject* FrameTexture;

  // see algorithm for more info
  virtual int FillInputPortInformation(int port, vtkInformation* info);

private:
  void InitParticle(vtkImageData*, vtkDataArray*, Particle*);
  void UpdateParticles(vtkImageData*, vtkDataArray*, vtkRenderer* ren);
  void DrawParticles(vtkRenderer* ren);
  void InitializeBuffers(vtkRenderer* ren);

  vtkStreamLinesMapper(const vtkStreamLinesMapper&) VTK_DELETE_FUNCTION;
  void operator=(const vtkStreamLinesMapper&) VTK_DELETE_FUNCTION;
};

#endif
