/*=========================================================================

   Program: ParaView
   Module:    pqSGExportStateWizard.cxx

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2.

   See License_v1.2.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

========================================================================*/
#include "pqImageOutputInfo.h"

#include <pqView.h>

#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPNGWriter.h>
#include <vtkSmartPointer.h>
#include <vtkUnsignedCharArray.h>
#include <vtksys/SystemTools.hxx>

#include "ui_pqImageOutputInfo.h"


//-----------------------------------------------------------------------------
pqImageOutputInfo::pqImageOutputInfo(QWidget* parent_)
: QWidget(parent_)
, Ui(new Ui::ImageOutputInfo())
{
  this->initialize(0, NULL, "");
}
//-----------------------------------------------------------------------------
pqImageOutputInfo::pqImageOutputInfo(QWidget *parentObject, Qt::WindowFlags
  parentFlags, pqView* view, QString& viewName)
: QWidget(parentObject, parentFlags)
, Ui(new Ui::ImageOutputInfo())
, View(view)
{
  this->initialize(parentFlags, view, viewName);
};

//-----------------------------------------------------------------------------
void pqImageOutputInfo::initialize(Qt::WindowFlags parentFlags, pqView* view,
  QString const & viewName)
{
  this->View = view;
  QWidget::setWindowFlags(parentFlags);
  this->Ui->setupUi(this);
  this->Ui->imageFileName->setText(viewName);

  QObject::connect(
    this->Ui->imageFileName, SIGNAL(editingFinished()),
    this, SLOT(updateImageFileName()));

  QObject::connect(
    this->Ui->imageType, SIGNAL(currentIndexChanged(const QString&)),
    this, SLOT(updateImageFileNameExtension(const QString&)));

  QObject::connect(
    this->Ui->cinemaExport, SIGNAL(currentIndexChanged(const QString&)),
    this, SLOT(updateCinemaType(const QString&)));

  this->setCinemaVisible(false);

  this->setupScreenshotInfo();
}

//-----------------------------------------------------------------------------
pqImageOutputInfo::~pqImageOutputInfo()
{

}

//-----------------------------------------------------------------------------
pqView* pqImageOutputInfo::getView()
{
  return this->View;
}

//-----------------------------------------------------------------------------
void pqImageOutputInfo::setView(pqView* const view)
{
  this->View = view;
}

//-----------------------------------------------------------------------------
QString pqImageOutputInfo::getImageFileName()
{
  return this->Ui->imageFileName->displayText();
}
//-----------------------------------------------------------------------------
void pqImageOutputInfo::hideFrequencyInput()
{
  this->Ui->imageWriteFrequency->hide();
  this->Ui->imageWriteFrequencyLabel->hide();
}

//-----------------------------------------------------------------------------
void pqImageOutputInfo::showFrequencyInput()
{
  this->Ui->imageWriteFrequency->show();
    this->Ui->imageWriteFrequencyLabel->show();
}
//-----------------------------------------------------------------------------
void pqImageOutputInfo::hideFitToScreen()
{
  this->Ui->fitToScreen->hide();
}

//-----------------------------------------------------------------------------
void pqImageOutputInfo::showFitToScreen()
{
  this->Ui->fitToScreen->show();
}
//-----------------------------------------------------------------------------
int pqImageOutputInfo::getWriteFrequency()
{
  return this->Ui->imageWriteFrequency->value();
}

//-----------------------------------------------------------------------------
bool pqImageOutputInfo::fitToScreen()
{
  return this->Ui->fitToScreen->isChecked();
}

//-----------------------------------------------------------------------------
int pqImageOutputInfo::getMagnification()
{
  return this->Ui->imageMagnification->value();
}

//-----------------------------------------------------------------------------
void pqImageOutputInfo::updateImageFileName()
{
  QString fileName = this->Ui->imageFileName->displayText();
  if(fileName.isNull() || fileName.isEmpty())
    {
    fileName = "image";
    }
  QRegExp regExp("\\.(png|bmp|ppm|tif|tiff|jpg|jpeg)$");
  if(fileName.contains(regExp) == 0)
    {
    fileName.append(".");
    fileName.append(this->Ui->imageType->currentText());
    }
  else
    {  // update imageType if it is different
    int extensionIndex = fileName.lastIndexOf(".");
    QString anExtension = fileName.right(fileName.size()-extensionIndex-1);
    int index = this->Ui->imageType->findText(anExtension);
    this->Ui->imageType->setCurrentIndex(index);
    fileName = this->Ui->imageFileName->displayText();
    }

  if(fileName.contains("%t") == 0)
    {
    fileName.insert(fileName.lastIndexOf("."), "_%t");
    }

  this->Ui->imageFileName->setText(fileName);
}

//-----------------------------------------------------------------------------
void pqImageOutputInfo::updateImageFileNameExtension(
  const QString& fileExtension)
{
  QString displayText = this->Ui->imageFileName->text();
  std::string newFileName = vtksys::SystemTools::GetFilenameWithoutExtension(
    displayText.toLocal8Bit().constData());

  newFileName.append(".");
  newFileName.append(fileExtension.toLocal8Bit().constData());
  this->Ui->imageFileName->setText(QString::fromStdString(newFileName));
}

//-----------------------------------------------------------------------------
void pqImageOutputInfo::setupScreenshotInfo()
{
  this->Ui->thumbnailLabel->setVisible(true);
  if(!this->View)
    {
    cerr << "No view has been set!" << '\n';
    return;
    }

  QSize viewSize = this->View->getSize();
  QSize thumbnailSize;
  if(viewSize.width() > viewSize.height())
    {
    thumbnailSize.setWidth(100);
    thumbnailSize.setHeight(100*viewSize.height()/viewSize.width());
    }
  else
    {
    thumbnailSize.setHeight(100);
    thumbnailSize.setWidth(100*viewSize.width()/viewSize.height());
    }
  if(this->View->widget()->isVisible())
    {
    vtkSmartPointer<vtkImageData> image;
    image.TakeReference(this->View->captureImage(thumbnailSize));
    vtkNew<vtkPNGWriter> pngWriter;
    pngWriter->SetInputData(image);
    pngWriter->WriteToMemoryOn();
    pngWriter->Update();
    pngWriter->Write();
    vtkUnsignedCharArray* result = pngWriter->GetResult();
    QPixmap thumbnail;
    thumbnail.loadFromData(
      result->GetPointer(0),
      result->GetNumberOfTuples()*result->GetNumberOfComponents(), "PNG");

    this->Ui->thumbnailLabel->setPixmap(thumbnail);
    }
}

//-----------------------------------------------------------------------------
void pqImageOutputInfo::setCinemaVisible(bool status, bool specASupport)
{
  if (status)
    {
    this->Ui->gbCinemaOptions->show();
    this->updateSpherical();
    }
  else
    this->Ui->gbCinemaOptions->hide();

  // Remove/ add options depending on what is supported
  if (!specASupport)
    {
    this->Ui->cinemaExport->clear();
    QStringList items;
    items << "None" << "Spherical";
    this->Ui->cinemaExport->addItems(items);
    }
  else
    {
      if (this->Ui->cinemaExport->count() < 3)
        {
        this->Ui->cinemaExport->clear();
        QStringList items;
        items << "None" << "Static" << "Spherical";
        this->Ui->cinemaExport->addItems(items);
        }
    }
}

//-----------------------------------------------------------------------------
void pqImageOutputInfo::updateCinemaType(
  const QString&)
{
  this->updateSpherical();
}

//------------------------------------------------------------------------------
void pqImageOutputInfo::updateSpherical()
{
  const QString& exportChoice =
    this->Ui->cinemaExport->currentText();
  if (exportChoice == "Spherical")
    {
    //this->Ui->wCameraOptions->show();
    this->Ui->wCameraOptions->setEnabled(true);
    }
  else
    {
    //this->Ui->wCameraOptions->hide();
    this->Ui->wCameraOptions->setEnabled(false);
    }
}

//------------------------------------------------------------------------------
const QString pqImageOutputInfo::getCameraType()
{
  return this->Ui->cinemaExport->currentText();
}

//------------------------------------------------------------------------------
double pqImageOutputInfo::getPhi()
{
  return this->Ui->phiResolution->value();
}

//------------------------------------------------------------------------------
double pqImageOutputInfo::getTheta()
{
  return this->Ui->thetaResolution->value();
}
