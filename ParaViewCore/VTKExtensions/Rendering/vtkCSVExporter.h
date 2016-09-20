/*=========================================================================

  Program:   ParaView
  Module:    vtkCSVExporter.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkCSVExporter - exporter used by certain views to export data as a CSV
// file.
// .SECTION Description
// This is used by vtkSMCSVExporterProxy to export the data shown in the
// spreadsheet view or chart views as a CSV. The reason this class simply
// doesn't use a vtkCSVWriter is that vtkCSVWriter is designed to write out a
// single vtkTable as CSV. For exporting data from views, generating this single
// vtkTable that can be exported is often time consuming or memory consuming or
// both. Having a special exporter helps us with that. It provides two sets of
// APIs:
//
// \li \c STREAM_ROWS: to use to stream a single large vtkTable as contiguous chunks where each chuck
// is a subset of the rows (ideal for use by vtkSpreadSheetView) viz. OpenFile,
// WriteHeader, WriteData (which can be repeated as many times as needed), and CloseFile,
// \li \c STREAM_COLUMNS: to use to add columns (idea for chart views) viz. OpenFile,
// AddColumn (which can be repeated), and CloseFile.
//
// One has to pick which mode the exporter is operating in during the OpenFile()
// call.
#ifndef vtkCSVExporter_h
#define vtkCSVExporter_h

#include "vtkObject.h"
#include "vtkPVVTKExtensionsRenderingModule.h" // needed for export macro

class vtkAbstractArray;
class vtkDataArray;
class vtkFieldData;

class VTKPVVTKEXTENSIONSRENDERING_EXPORT vtkCSVExporter : public vtkObject
{
public:
  static vtkCSVExporter* New();
  vtkTypeMacro(vtkCSVExporter, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Get/Set the filename for the file.
  vtkSetStringMacro(FileName);
  vtkGetStringMacro(FileName);

  // Description:
  // Get/Set the delimiter use to separate fields ("," by default.)
  vtkSetStringMacro(FieldDelimiter);
  vtkGetStringMacro(FieldDelimiter);

  enum ExporterModes
    {
    STREAM_ROWS,
    STREAM_COLUMNS
    };

  // Description:
  // Open the file and set mode in which the exporter is operating.
  bool Open(ExporterModes mode=STREAM_ROWS);

  // Description:
  // Closes the file cleanly. Call this at the end to close the file and dump
  // out any cached data.
  void Close();

  // Description
  // Same as Close except deletes the file, if created. This is useful to
  // interrupt the exporting on failure.
  void Abort();

  // Description:
  // In STREAM_ROWS mode, use these methods to write column headers once using
  // WriteHeader and then use WriteData as many times as needed to write out
  // rows.
  void WriteHeader(vtkFieldData*);
  void WriteData(vtkFieldData*);

  // Description:
  // In STREAM_COLUMNS mode, use this method to add a column (\c yarray). One
  // can assign it a name different the the name of the array using \c
  // yarrayname. If \c xarray is not NULL, then is used as the row-id. This
  // makes it possible to add multiple columns with varying number of samples.
  // The final output will have empty cells for missing values.
  void AddColumn(vtkAbstractArray* yarray, const char* yarrayname=NULL, vtkDataArray* xarray=NULL);

protected:
  vtkCSVExporter();
  ~vtkCSVExporter();

  char* FileName;
  char* FieldDelimiter;
  ofstream *FileStream;
  ExporterModes Mode;

private:
  vtkCSVExporter(const vtkCSVExporter&); // Not implemented
  void operator=(const vtkCSVExporter&); // Not implemented

  class vtkInternals;
  vtkInternals* Internals;

};

#endif

