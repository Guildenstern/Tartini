/***************************************************************************
                          savedialog.cpp  -  description
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

#include "savedialog.h"
#include "gdata.h"
#include <QtGui/QVBoxLayout>
#include <QtGui/QCheckBox>
#include <QtCore/QSettings>

SaveDialog::SaveDialog(QWidget * parent)
 : QFileDialog(parent, "Save file", QDir::convertSeparators(
           gdata->qsettings->value("Dialogs/saveFilesFolder", QDir::currentPath()).toString()),
               "Wave files (*.wav)")
{
  setWindowTitle("Choose a filename to save under");
  setAcceptMode(AcceptSave);
  setFileMode(QFileDialog::AnyFile);

  QVBoxLayout* baseLayout = new QVBoxLayout();
  appendWavCheckBox = new QCheckBox("Append .wav extension if needed");
  rememberFolderCheckBox = new QCheckBox("Remember current folder");
  appendWavCheckBox->setChecked(gdata->qsettings->value("Dialogs/appendWav", true).toBool());
  rememberFolderCheckBox->setChecked(gdata->qsettings->value("Dialogs/rememberSaveFolder", true).toBool());
  baseLayout->addSpacing(10);
  baseLayout->addWidget(appendWavCheckBox);
  baseLayout->addWidget(rememberFolderCheckBox);
  layout()->addItem(baseLayout);
}

SaveDialog::~SaveDialog()
{
}

void SaveDialog::accept()
{
  bool remember = rememberFolderCheckBox->isChecked();
  gdata->qsettings->setValue("Dialogs/rememberSaveFolder", remember);
  if(remember == true) {
    const QDir curDir = directory();
    gdata->qsettings->setValue("Dialogs/saveFilesFolder", curDir.absolutePath());
  }
  bool appendWav = appendWavCheckBox->isChecked();
  gdata->qsettings->setValue("Dialogs/appendWav", appendWav);
  if(appendWav == true) {
    QStringList sl = selectedFiles();
    for (int i = 0; i < sl.length(); ++i) {
        if (!sl[i].toLower().endsWith(".wav")) {
            sl[i] += ".wav";
        }
        selectFile(sl[i]);
    }
  }
  QFileDialog::accept();
}

QString SaveDialog::getSaveWavFileName(QWidget *parent)
{
  SaveDialog d(parent);
  if(d.exec() != QDialog::Accepted) return QString();
  QStringList sl = d.selectedFiles();
  if (sl.length() > 0) {
      return sl[0];
  }
  return QString();
}

