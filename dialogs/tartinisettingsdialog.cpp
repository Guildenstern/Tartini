/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you wish to add, delete or rename functions or slots use
** Qt Designer which will update this file, preserving your code. Create an
** init() function in place of a constructor, and a destroy() function in
** place of a destructor.
*****************************************************************************/

#include "tartinisettingsdialog.h"
#include "gdata.h"
#include "audio_stream.h"
#include "mainwindow.h"
#include <QtGui/QFileDialog>
#include <QtGui/QColorDialog>
#include <QtCore/QSettings>

TartiniSettingsDialog::TartiniSettingsDialog(QWidget *parent)
  : QDialog(parent)
{
  setupUi(this);

  connect(okButton, SIGNAL(clicked()), this, SLOT(saveSettings()));
  connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

  connect( backgroundButton, SIGNAL( clicked() ), this, SLOT( getBackgroundColor() ) );
  connect( shading1Button, SIGNAL( clicked() ), this, SLOT( getShading1Color() ) );
  connect( shading2Button, SIGNAL( clicked() ), this, SLOT( getShading2Color() ) );
  connect( fileNumberOfDigits, SIGNAL( valueChanged(int) ), this, SLOT( fileNameChanged() ) );
  connect( fileGeneratingNumber, SIGNAL( valueChanged(int) ), this, SLOT( fileNameChanged() ) );
  connect( filenameGeneratingString, SIGNAL( textChanged(const QString&) ), this, SLOT( fileNameChanged() ) );
  connect( chooseTempFolderButton, SIGNAL( clicked() ), this, SLOT( changeTempFolder() ) );
  connect( noteRangeChoice, SIGNAL( activated(int) ), this, SLOT( onNoteRangeChoice(int) ) );
  connect( resetDefaultsButton, SIGNAL( clicked() ), this, SLOT( resetDefaults() ) );

  init();
}

namespace {
bool isA(const QObject* object, const char* name) {
    return object && strcmp(object->metaObject()->className(), name) == 0;
}
}

void TartiniSettingsDialog::loadSetting(QObject *obj, const QString &group)
{
  QString key = obj->objectName();
  QString fullKey = group + "/" + key;

  if(isA(obj, "QGroupBox")) {
    //Iterate over the groupBox's children
    const QList<QObject*> &widgets = obj->children();
    for(QList<QObject*>::const_iterator it = widgets.begin(); it < widgets.end(); ++it) {
      loadSetting(*it, group);
    }
  } else if(isA(obj, "QLineEdit")) {
    ((QLineEdit*)obj)->setText(gdata->qsettings->value(fullKey).toString());
  } else if(isA(obj, "QComboBox")) {
      QComboBox* box = (QComboBox*)obj;
      box->setItemText(box->currentIndex(), gdata->qsettings->value(fullKey).toString());
  } else if(isA(obj, "QPushButton") && ((QPushButton*)obj)->isCheckable()) {
    ((QPushButton*)obj)->setChecked(gdata->qsettings->value(fullKey).toBool());
  } else if(isA(obj, "QCheckBox")) {
    ((QCheckBox*)obj)->setChecked(gdata->qsettings->value(fullKey).toBool());
  } else if(isA(obj, "QSpinBox")) {
    ((QSpinBox*)obj)->setValue(gdata->qsettings->value(fullKey).toInt());
  } else if(isA(obj, "QFrame")) {
      QFrame* frame = (QFrame*)obj;
      QPalette palette = frame->palette();
    QColor color;
    color.setNamedColor(gdata->qsettings->value(fullKey).toString());
    palette.setColor(backgroundRole(), color);
    frame->setPalette(palette);
  }
}

void TartiniSettingsDialog::init()
{
  // Go through all the categories on the left, and insert all the preferences we can into the fields.
  // Combo boxes must be done separately.
  soundInput->clear();
  soundInput->insertItems(0, AudioStream::getInputDeviceNames());
  soundOutput->clear();
  soundOutput->insertItems(0, AudioStream::getOutputDeviceNames());
  
  QString group;
  //Iterate over all groups
  for (int i = 0; i < tabWidget->count(); i++) {
    //Iterate over all widgets in the current group and load their settings
    group = tabWidget->tabText(i);
    const QList<QObject*> &widgets = tabWidget->widget(i)->children();
    for(QList<QObject*>::const_iterator it=widgets.begin(); it < widgets.end(); ++it) {
      loadSetting(*it, group);
    }
  }
  checkAnalysisEnabled();
}

QString TartiniSettingsDialog::getPath(const QString initialPath)
{
  return QFileDialog::getExistingDirectory(this, "Choose a directory", initialPath);
}

void TartiniSettingsDialog::changeTempFolder()
{
  if(tempFilesFolder) {
    QString path = getPath(tempFilesFolder->text());
    if (path != "") tempFilesFolder->setText(QDir::convertSeparators(path));
  }
}

void TartiniSettingsDialog::fileNameChanged()
{
  QString filename;
  int digits = fileNumberOfDigits->value();
  if(digits == 0) {
    filename.sprintf("%s.wav", filenameGeneratingString->text().toLatin1().data());
  } else {
    filename.sprintf("%s%0*d.wav", filenameGeneratingString->text().toLatin1().data(), digits, fileGeneratingNumber->value());
  }
  filenameExample->setText(filename);
}

namespace {
void setColor(QFrame *frame) {
    QPalette palette(frame->palette());
    QColor color = QColorDialog::getColor(palette.background().color(), frame,
                                          "Background Color");
  if(color.isValid()) {
      palette.setColor(frame->backgroundRole(), color);
    frame->setPalette(palette);
  }
}
}

void TartiniSettingsDialog::getBackgroundColor()
{
    setColor(theBackgroundColor);
}

void TartiniSettingsDialog::getShading1Color()
{
  setColor(shading1Color);
}

void TartiniSettingsDialog::getShading2Color()
{
    setColor(shading2Color);
}

void TartiniSettingsDialog::saveSetting(QObject *obj, const QString group)
{
  QString key = obj->objectName();
  QString fullKey = group + "/" + key;

  if(isA(obj, "QGroupBox")) {
    //Iterate over the groupBox's children
    const QList<QObject*> &widgets = obj->children();
    for(QList<QObject*>::const_iterator it=widgets.begin(); it < widgets.end(); ++it) {
      saveSetting(*it, group);
    }
  } else if(isA(obj, "QLineEdit")) {
    gdata->qsettings->setValue(fullKey, ((QLineEdit*)obj)->text());
  } else if(isA(obj, "QComboBox")) {
    gdata->qsettings->setValue(fullKey, ((QComboBox*)obj)->currentText());
  } else if(isA(obj, "QPushButton") && ((QPushButton*)obj)->isCheckable()) {
    gdata->qsettings->setValue(fullKey, ((QPushButton*)obj)->isChecked());
  } else if(isA(obj, "QCheckBox")) {
    gdata->qsettings->setValue(fullKey, ((QCheckBox*)obj)->isChecked());
  } else if(isA(obj, "QSpinBox")) {
    gdata->qsettings->setValue(fullKey, ((QSpinBox*)obj)->value());
  } else if(isA(obj, "QFrame")) {
    gdata->qsettings->setValue(fullKey, ((QFrame*)obj)->palette().color(backgroundRole()).name());
  }
}

void TartiniSettingsDialog::saveSettings()
{
  // Go through all the categories on the left, and save all the preferences we can from the fields.
  // Combo boxes must be done separately.
  QString group;
  //QObject *obj;
  //const QObjectList *widgets;
  //Iterate over all the groups
  for(int i = 0; i < tabWidget->count(); i++) {
    //Iterate over all widgets in the current group and save their settings
    group = tabWidget->tabText(i);
    const QList<QObject*> &widgets = tabWidget->widget(i)->children();
    for(QList<QObject*>::const_iterator it=widgets.begin(); it < widgets.end(); ++it) {
      saveSetting(*it, group);
    }
  }

  gdata->qsettings->sync();
	
  QApplication::postEvent(gdata->mainWindow, new QEvent(AudioThread::SETTINGS_CHANGED));
	
  TartiniSettingsDialog::accept();

}

void TartiniSettingsDialog::checkAnalysisEnabled()
{
  QComboBox *noteRangeChoice = tabWidget->widget(0)->findChild<QComboBox *>("noteRangeChoice");
  myassert(noteRangeChoice);
  
  int choice = noteRangeChoice->currentIndex();

  QGroupBox *bufferSizeGroupBox = tabWidget->widget(2)->findChild<QGroupBox*>("bufferSizeGroupBox");
  myassert(bufferSizeGroupBox);
  QGroupBox *stepSizeGroupBox = tabWidget->widget(2)->findChild<QGroupBox*>("stepSizeGroupBox");
  myassert(stepSizeGroupBox);
  
  if(choice == 0) {
    bufferSizeGroupBox->setEnabled(true);
    stepSizeGroupBox->setEnabled(true);
  } else {
    bufferSizeGroupBox->setEnabled(false);
    stepSizeGroupBox->setEnabled(false);
  }
}

void TartiniSettingsDialog::onNoteRangeChoice(int choice)
{
  QSpinBox *bufferSizeValue = tabWidget->widget(2)->findChild<QSpinBox*>("bufferSizeValue");
  myassert(bufferSizeValue);
  QComboBox *bufferSizeUnit = tabWidget->widget(2)->findChild<QComboBox*>("bufferSizeUnit");
  myassert(bufferSizeUnit);
  QCheckBox *bufferSizeRound = tabWidget->widget(2)->findChild<QCheckBox*>("bufferSizeRound");
  myassert(bufferSizeRound);
  QSpinBox *stepSizeValue = tabWidget->widget(2)->findChild<QSpinBox*>("stepSizeValue");
  myassert(stepSizeValue);
  QComboBox *stepSizeUnit = tabWidget->widget(2)->findChild<QComboBox*>("stepSizeUnit");
  myassert(stepSizeUnit);
  QCheckBox *stepSizeRound = tabWidget->widget(2)->findChild<QCheckBox*>("stepSizeRound");
  myassert(stepSizeRound);

  switch(choice) {
    case 1:
        bufferSizeValue->setValue(96);
        stepSizeValue->setValue(48);
      break;
    case 2:
        bufferSizeValue->setValue(48);
        stepSizeValue->setValue(24);
      break;
    case 3:
        bufferSizeValue->setValue(24);
        stepSizeValue->setValue(12);
      break;
    case 4:
        bufferSizeValue->setValue(12);
        stepSizeValue->setValue(6);
      break;
  }
  if(choice > 0) {
    bufferSizeUnit->setItemText(bufferSizeUnit->currentIndex(), "milli-seconds");
    stepSizeUnit->setItemText(bufferSizeUnit->currentIndex(), "milli-seconds");
    bufferSizeRound->setChecked(true);
    stepSizeRound->setChecked(true);
  }
  checkAnalysisEnabled();
}

#define SetIfMissing(key, value) \
  if(!s->contains(key)) s->setValue(key, value)

void TartiniSettingsDialog::setUnknownsToDefault(QSettings *s)
{
  SetIfMissing("General/bindOpenSaveFolders", true);
  SetIfMissing("General/tempFilesFolder", QDir::convertSeparators(QDir::currentPath()));
  SetIfMissing("General/filenameGeneratingString", "Untitled");
  SetIfMissing("General/fileGeneratingNumber", 1);
  SetIfMissing("General/fileNumberOfDigits", 2);
  SetIfMissing("General/noteRangeChoice", "Notes F1 and higher - Cello, Guitar, Bass Clarinet, General sounds ...");

  SetIfMissing("View/autoFollow", false);
  SetIfMissing("View/backgroundShading", true);
  SetIfMissing("View/freqA", 440);

  SetIfMissing("Sound/soundInput", "Default");
  SetIfMissing("Sound/soundOutput", "Default");
  SetIfMissing("Sound/numberOfBuffers", 4);
  SetIfMissing("Sound/numberOfChannels", "Mono");
  SetIfMissing("Sound/sampleRate", 44100);
  SetIfMissing("Sound/bitsPerSample", 16);
  SetIfMissing("Sound/muteOutput", true);

  SetIfMissing("Analysis/bufferSizeValue", 48);
  SetIfMissing("Analysis/bufferSizeUnit", "milli-seconds");
  SetIfMissing("Analysis/bufferSizeRound", true);
  SetIfMissing("Analysis/stepSizeValue", 24);
  SetIfMissing("Analysis/stepSizeUnit", "milli-seconds");
  SetIfMissing("Analysis/stepSizeRound", true);
  SetIfMissing("Analysis/doingHarmonicAnalysis", true);
  SetIfMissing("Analysis/doingFreqAnalysis", true);
  SetIfMissing("Analysis/doingEqualLoudness", true);
  SetIfMissing("Analysis/doingAutoNoiseFloor", true);
  SetIfMissing("Analysis/doingDetailedPitch", true);
  SetIfMissing("Analysis/analysisType", "MPM");
  SetIfMissing("Analysis/thresholdValue", 93);

  SetIfMissing("Display/fastUpdateSpeed", 75);
  SetIfMissing("Display/slowUpdateSpeed", 150);
  SetIfMissing("Display/theBackgroundColor", "#BBCDE2");
  SetIfMissing("Display/shading1Color", "#BBCDEF");
  SetIfMissing("Display/shading2Color", "#CBCDE2");
  SetIfMissing("Display/displayBackgroundShading", true);
  SetIfMissing("Display/useTopLevelWidgets", false);

  SetIfMissing("Dialogs/rememberOpenFolder", true);
  SetIfMissing("Dialogs/openFilesFolder", QDir::convertSeparators(QDir::currentPath()));
  SetIfMissing("Dialogs/rememberSaveFolder", true);
  SetIfMissing("Dialogs/saveFilesFolder", QDir::convertSeparators(QDir::currentPath()));
  SetIfMissing("Dialogs/appendWav", true);

  SetIfMissing("Advanced/showMeanVarianceBars", false);
  SetIfMissing("Advanced/savingMode", "Ask when closing unsaved files (normal)");
  SetIfMissing("Advanced/vibratoSineStyle", false);
  SetIfMissing("Advanced/mouseWheelZooms", false);
}

void TartiniSettingsDialog::resetDefaults()
{
  gdata->qsettings->clear();
  setUnknownsToDefault(gdata->qsettings);
  gdata->qsettings->sync();
  init();
}
