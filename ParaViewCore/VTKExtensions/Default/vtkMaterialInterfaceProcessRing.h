/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile$

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkMaterialInterfaceProcessRing
// .SECTION Description
// Data structure used to distribute work amongst available
// processes. The buffer can be intialized from a process
// priority queue such that only those processes with loading
// less than a specified tolerance are included.

#ifndef vtkMaterialInterfaceProcessRing_h
#define vtkMaterialInterfaceProcessRing_h

#include <vector> // needed for Initialize()
#include "vtkPVVTKExtensionsDefaultModule.h" //needed for exports
#include "vtkMaterialInterfaceProcessLoading.h"

class VTKPVVTKEXTENSIONSDEFAULT_EXPORT vtkMaterialInterfaceProcessRing
{
public:
  // Description:
  vtkMaterialInterfaceProcessRing();
  // Description:
  ~vtkMaterialInterfaceProcessRing();
  // Description:
  // Return the object to an empty state.
  void Clear();
  // Description:
  // Size buffer and point to first element.
  void Initialize(int nProcs);
  // Description:
  // Build from a process loading from a sorted
  // vector of process loading items.
  void Initialize(
      std::vector<vtkMaterialInterfaceProcessLoading> &Q,
      vtkIdType upperLoadingBound);
  // Description:
  // Get the next process id from the ring.
  vtkIdType GetNextId();
  // Description:
  // Print the state of the ring.
  void Print();

private:
  vtkIdType NextElement;
  vtkIdType BufferSize;
  class BufferContainer;
  BufferContainer *Buffer;
};
#endif

// VTK-HeaderTest-Exclude: vtkMaterialInterfaceProcessRing.h
