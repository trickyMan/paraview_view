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
/**
 * @class   vtkStreamLinesRepresentation
 * @brief   Representation for showing ImageData using live streamlines.
 *
 */

#ifndef vtkStreamLinesRepresentation_h
#define vtkStreamLinesRepresentation_h

#include "vtkPVDataRepresentation.h"
#include "vtkNew.h"                               // needed for vtkNew.

class vtkColorTransferFunction;
class vtkExtentTranslator;
class vtkFixedPointVolumeRayCastMapper;
class vtkImageData;
class vtkInformation;
class vtkInformationRequestKey;
class vtkOutlineSource;
class vtkPExtentTranslator;
class vtkPiecewiseFunction;
class vtkPolyDataMapper;
class vtkPVCacheKeeper;
class vtkPVLODActor;
class vtkScalarsToColors;
class vtkStreamLinesMapper;
class vtkProperty;


class VTK_EXPORT vtkStreamLinesRepresentation : public vtkPVDataRepresentation
{
public:
  static vtkStreamLinesRepresentation* New();
  vtkTypeMacro(vtkStreamLinesRepresentation, vtkPVDataRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent);

  /**
   * vtkAlgorithm::ProcessRequest() equivalent for rendering passes. This is
   * typically called by the vtkView to request meta-data from the
   * representations or ask them to perform certain tasks e.g.
   * PrepareForRendering.
   */
  virtual int ProcessViewRequest(
    vtkInformationRequestKey* request_type, vtkInformation* inInfo, vtkInformation* outInfo);

  /**
   * This needs to be called on all instances of vtkGeometryRepresentation when
   * the input is modified. This is essential since the geometry filter does not
   * have any real-input on the client side which messes with the Update
   * requests.
   */
  virtual void MarkModified();

  /**
   * Get/Set the visibility for this representation. When the visibility of
   * representation of false, all view passes are ignored.
   */
  virtual void SetVisibility(bool val);


  //***************************************************************************
  // Forwarded to vtkProperty.
  virtual void SetAmbientColor(double r, double g, double b);
  virtual void SetColor(double r, double g, double b);
  virtual void SetDiffuseColor(double r, double g, double b);
  virtual void SetEdgeColor(double r, double g, double b);
  virtual void SetInterpolation(int val);
  virtual void SetLineWidth(double val);
  virtual void SetOpacity(double val);
  virtual void SetPointSize(double val);
  virtual void SetSpecularColor(double r, double g, double b);
  virtual void SetSpecularPower(double val);

  //***************************************************************************
  // Forwarded to vtkStreamLinesMapper
  void SetAlpha(double val);
  void SetStepLength(double val);
  void SetNumberOfParticles(int val);
  void SetMaxTimeToLive(int val);

  void SetInputVectors(int, int, int, int attributeMode, const char* name);

  //***************************************************************************
  // Forwarded to Mapper and LODMapper.
  virtual void SetInterpolateScalarsBeforeMapping(int val);
  virtual void SetLookupTable(vtkScalarsToColors* val);

  //@{
  /**
   * Sets if scalars are mapped through a color-map or are used
   * directly as colors.
   * 0 maps to VTK_COLOR_MODE_DIRECT_SCALARS
   * 1 maps to VTK_COLOR_MODE_MAP_SCALARS
   * @see vtkScalarsToColors::MapScalars
   */
  virtual void SetMapScalars(int val);

  /**
   * Fill input port information.
   */
  virtual int FillInputPortInformation(int port, vtkInformation* info);

  /**
   * Provides access to the actor used by this representation.
   */
  vtkPVLODActor* GetActor() { return this->Actor; }

  /**
   * Convenience method to get the array name used to scalar color with.
   */
  const char* GetColorArrayName();

  // Description:
  // Set the input data arrays that this algorithm will process. Overridden to
  // pass the array selection to the mapper.
  virtual void SetInputArrayToProcess(
    int idx, int port, int connection, int fieldAssociation, const char* name);
  virtual void SetInputArrayToProcess(
    int idx, int port, int connection, int fieldAssociation, int fieldAttributeType)
  {
    this->Superclass::SetInputArrayToProcess(
      idx, port, connection, fieldAssociation, fieldAttributeType);
  }
  virtual void SetInputArrayToProcess(int idx, vtkInformation* info)
  {
    this->Superclass::SetInputArrayToProcess(idx, info);
  }
  virtual void SetInputArrayToProcess(int idx, int port, int connection,
    const char* fieldAssociation, const char* attributeTypeorName)
  {
    this->Superclass::SetInputArrayToProcess(
      idx, port, connection, fieldAssociation, attributeTypeorName);
  }

protected:
  vtkStreamLinesRepresentation();
  ~vtkStreamLinesRepresentation();

  virtual int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*);

  /**
   * Adds the representation to the view.  This is called from
   * vtkView::AddRepresentation().  Subclasses should override this method.
   * Returns true if the addition succeeds.
   */
  virtual bool AddToView(vtkView* view);

  /**
   * Removes the representation to the view.  This is called from
   * vtkView::RemoveRepresentation().  Subclasses should override this method.
   * Returns true if the removal succeeds.
   */
  virtual bool RemoveFromView(vtkView* view);

  /**
   * Overridden to check with the vtkPVCacheKeeper to see if the key is cached.
   */
  virtual bool IsCached(double cache_key);

  /**
   * Passes on parameters to the active volume mapper
   */
  virtual void UpdateMapperParameters();

  /** Description:
   * Overridden method to set parameters on vtkProperty and vtkMapper.
   */
  void UpdateColoringParameters();

  /**
   * Used in ConvertSelection to locate the rendered prop.
   */
  virtual vtkPVLODActor* GetRenderedProp() { return this->Actor; };

  vtkImageData* Cache;
  vtkAlgorithm* MBMerger;
  vtkPVCacheKeeper* CacheKeeper;
  vtkStreamLinesMapper* StreamLinesMapper;
  vtkProperty* Property;
  vtkPVLODActor* Actor;

  vtkOutlineSource* OutlineSource;
  vtkPolyDataMapper* OutlineMapper;

  unsigned long DataSize;
  double DataBounds[6];

  // meta-data about the input image to pass on to render view for hints
  // when redistributing data.
  vtkNew<vtkPExtentTranslator> PExtentTranslator;
  double Origin[3];
  double Spacing[3];
  int WholeExtent[6];

private:
  vtkStreamLinesRepresentation(const vtkStreamLinesRepresentation&) VTK_DELETE_FUNCTION;
  void operator=(const vtkStreamLinesRepresentation&) VTK_DELETE_FUNCTION;

};

#endif
