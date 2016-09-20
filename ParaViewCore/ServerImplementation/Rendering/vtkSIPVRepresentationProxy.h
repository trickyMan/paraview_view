/*=========================================================================

  Program:   ParaView
  Module:    vtkSIPVRepresentationProxy.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkSIPVRepresentationProxy
// .SECTION Description
// vtkSIPVRepresentationProxy is the helper for vtkSMPVRepresentationProxy.

#ifndef vtkSIPVRepresentationProxy_h
#define vtkSIPVRepresentationProxy_h

#include "vtkPVServerImplementationRenderingModule.h" //needed for exports
#include "vtkSIProxy.h"

class VTKPVSERVERIMPLEMENTATIONRENDERING_EXPORT vtkSIPVRepresentationProxy : public vtkSIProxy
{
public:
  static vtkSIPVRepresentationProxy* New();
  vtkTypeMacro(vtkSIPVRepresentationProxy, vtkSIProxy);
  void PrintSelf(ostream& os, vtkIndent indent);

  virtual void AboutToDelete();

protected:
  vtkSIPVRepresentationProxy();
  ~vtkSIPVRepresentationProxy();

  // Description:
  // Parses the XML to create property/subproxy helpers.
  // Overridden to parse all the "RepresentationType" elements.
  virtual bool ReadXMLAttributes(vtkPVXMLElement* element);

private:
  vtkSIPVRepresentationProxy(const vtkSIPVRepresentationProxy&); // Not implemented
  void operator=(const vtkSIPVRepresentationProxy&); // Not implemented

  void OnVTKObjectModified();

  class vtkInternals;
  vtkInternals* Internals;

};

#endif
