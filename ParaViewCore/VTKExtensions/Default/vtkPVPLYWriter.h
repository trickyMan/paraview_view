/*=========================================================================

  Program:   ParaView
  Module:    vtkPVPLYWriter.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkPVPLYWriter - provides a ParaView friendly API for vtkPLYWriter.
// .SECTION Description
// This is a facade for vtkPLYWriter that provides an API more amiable to the
// GUI shown in the ParaView application.

#ifndef vtkPVPLYWriter_h
#define vtkPVPLYWriter_h

#include "vtkPVVTKExtensionsDefaultModule.h" //needed for exports
#include "vtkNew.h" // needed for vtkNew
#include "vtkSmartPointer.h" // needed for vtkSmartPointer
#include "vtkWriter.h"

class vtkPLYWriter;
class vtkScalarsToColors;

class VTKPVVTKEXTENSIONSDEFAULT_EXPORT vtkPVPLYWriter : public vtkWriter
{
public:
  static vtkPVPLYWriter* New();
  vtkTypeMacro(vtkPVPLYWriter, vtkWriter);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Enable coloring.
  vtkSetMacro(EnableColoring, bool);
  vtkGetMacro(EnableColoring, bool);

  // Description:
  // If the file type is binary, then the user can specify which
  // byte order to use (little versus big endian).
  void SetDataByteOrder(int dbo);

  // Description:
  // Specify file type (ASCII or BINARY) for vtk data file.
  void SetFileType(int ftype);

  // Description:
  // Specify file name of vtk polygon data file to write.
  void SetFileName(const char* fname);

  // Description:
  // A lookup table can be specified in order to convert data arrays to
  // RGBA colors.
  void SetLookupTable(vtkScalarsToColors* lut);

protected:
  vtkPVPLYWriter();
  ~vtkPVPLYWriter();

  virtual int FillInputPortInformation(int port, vtkInformation *info);
  virtual void WriteData();

  bool EnableColoring;
  vtkNew<vtkPLYWriter> Writer;
  vtkSmartPointer<vtkScalarsToColors> LookupTable;
private:
  vtkPVPLYWriter(const vtkPVPLYWriter&); // Not implemented
  void operator=(const vtkPVPLYWriter&); // Not implemented

};

#endif
