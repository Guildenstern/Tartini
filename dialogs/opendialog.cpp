/***************************************************************************
                          opendialog.cpp  -  description
                             -------------------
    begin                : Feb 2005
    copyright            : (C) 2005 by Philip McLeod
    email                : pmcleod@cs.otago.ac.nz
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
 ***************************************************************************/

#include "opendialog.h"
#include "gdata.h"
#include <QtCore/QSettings>

OpenDialog::OpenDialog(QWidget * parent)
 : QFileDialog(parent, "Open File", QDir::convertSeparators(gdata->qsettings->value("Dialogs/openFilesFolder", QDir::currentPath()).toString()),
 "Wave files (*.wav)")
{
  setWindowTitle("Choose a file to open");
  setFileMode(QFileDialog::ExistingFile);
}

OpenDialog::~OpenDialog()
{
}

void OpenDialog::accept()
{
  QFileDialog::accept();
}

QString OpenDialog::getOpenWavFileName(QWidget *parent)
{
  OpenDialog d(parent);
  if(d.exec() != QDialog::Accepted) return QString();
  QStringList sl = d.selectedFiles();
  if (sl.length() > 0) {
      return sl[0];
  }
  return QString();
}
