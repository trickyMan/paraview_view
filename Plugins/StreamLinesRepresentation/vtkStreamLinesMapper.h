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

#include "vtkRenderingCoreModule.h" // For export macro
#include "vtkAbstractMapper3D.h"

class vtkRenderer;
class vtkVolume;
class vtkWindow;
class vtkDataSet;

class VTKRENDERINGCORE_EXPORT vtkStreamLinesMapper : public vtkAbstractVolumeMapper
{
public:
  vtkTypeMacro(vtkStreamLinesMapper,vtkAbstractMapper3D);
  void PrintSelf( ostream& os, vtkIndent indent );

    //@{
    /**
     * Get/Set the number of integration steps in each direction.
     */
  void SetNumberOfSteps(int val);
  vtkGetMacro(NumberOfSteps, int);
    //@}

  //@{
  /**
   * Get/Set the step size (in pixels).
   */
  void SetStepSize(double val);
  vtkGetMacro(StepSize, double);
  //@}

  /**
   * WARNING: INTERNAL METHOD - NOT INTENDED FOR GENERAL USE
   * DO NOT USE THIS METHOD OUTSIDE OF THE RENDERING PROCESS
   * Render the volume
   */
  virtual void Render(vtkRenderer *ren, vtkVolume *vol);

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

  int NumberOfSteps;
  double StepSize;

  // see algorithm for more info
  virtual int FillInputPortInformation(int port, vtkInformation* info);


private:
  vtkStreamLinesMapper(const vtkStreamLinesMapper&) VTK_DELETE_FUNCTION;
  void operator=(const vtkStreamLinesMapper&) VTK_DELETE_FUNCTION;
};

#endif
