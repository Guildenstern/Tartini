/***************************************************************************
                          freqview.cpp  -  description
                             -------------------
    begin                : Fri Dec 10 2004
    copyright            : (C) 2004-2005 by Philip McLeod
    email                : pmcleod@cs.otago.ac.nz
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
 ***************************************************************************/
#include "freqview.h"
#include "view.h"
#include "gdata.h"
#include "timeaxis.h"
#include "freqwidgetGL.h"
#include "myscrollbar.h"
#include "amplitudewidget.h"
#include <qwt/qwt_wheel.h>
#include <QtGui/QBoxLayout>
#include <QtGui/QSplitter>
#include <QtGui/QToolTip>
#include <QtGui/QComboBox>
#include <QtGui/QSizeGrip>

FreqView::FreqView(int viewID_, QWidget *parent)
  : ViewWidget(viewID_, parent)
{
  View *view = gdata->view;

  QBoxLayout *mainLayout = new QVBoxLayout();
  setLayout(mainLayout);
  mainLayout->setSizeConstraint(QLayout::SetNoConstraint);

  QSplitter *splitter = new QSplitter(Qt::Vertical);
  mainLayout->addWidget(splitter);
  QWidget *topWidget = new QWidget();
  splitter->addWidget(topWidget);
  QBoxLayout *topLayout = new QHBoxLayout();
  topWidget->setLayout(topLayout);
  QBoxLayout *topLeftLayout = new QVBoxLayout();
  topLayout->addLayout(topLeftLayout);
  
  timeAxis = new TimeAxis(topWidget, gdata->leftTime(), gdata->rightTime(), true);
  topLeftLayout->addWidget(timeAxis);
  timeAxis->setWhatsThis("The time in seconds");

  QFrame *freqFrame = new QFrame();
  topLeftLayout->addWidget(freqFrame);
  freqFrame->setFrameStyle(QFrame::WinPanel | QFrame::Sunken);
  QVBoxLayout *freqFrameLayout = new QVBoxLayout();
  freqFrame->setLayout(freqFrameLayout);
  freqWidgetGL = new FreqWidgetGL(NULL);
  freqFrameLayout->addWidget(freqWidgetGL);
  freqWidgetGL->setWhatsThis("The line represents the musical pitch of the sound. A higher pitch moves up, with note names shown at the left, with octave numbers. "
    "The black vertical line shows the current time. This line's position can be moved. "
    "Pitch-lines are drawn connected only because they change a small amount over a small time step. "
    "Note: This may cause semi-tone note changes appear to be joined. Clicking the background and dragging moves the view. "
    "Clicking a pitch-line selects it. Mouse-Wheel scrolls. Shift-Mouse-Wheel zooms");
  freqFrameLayout->setMargin(0);
  freqFrameLayout->setSpacing(0);

  QVBoxLayout *topRightLayout = new QVBoxLayout();
  topLayout->addLayout(topRightLayout);
  
  freqWheelY = new QwtWheel();
  topRightLayout->addWidget(freqWheelY, 0);
  freqWheelY->setOrientation(Qt::Vertical);
  freqWheelY->setWheelWidth(14);
  freqWheelY->setRange(1.6, 5.0, 0.001, 1);
  freqWheelY->setValue(view->logZoomY());
  freqWheelY->setToolTip("Zoom pitch contour view vertically");
  topRightLayout->addSpacing(20);
  
  //Create the vertical scrollbar
  freqScrollBar = new MyScrollBar(0, gdata->topPitch()-view->viewHeight(), 0.5, view->viewHeight(), 0, 20, Qt::Vertical, topWidget);
  topRightLayout->addWidget(freqScrollBar, 4);
  freqScrollBar->setValue(gdata->topPitch()-view->viewHeight()-view->viewBottom());

  //------------bottom half------------------
  
  QWidget *bottomWidget = new QWidget();
  splitter->addWidget(bottomWidget);
  QBoxLayout *bottomLayout = new QVBoxLayout();
  bottomWidget->setLayout(bottomLayout);
  QBoxLayout *bottomTopLayout = new QHBoxLayout();
  bottomLayout->addLayout(bottomTopLayout);

  QFrame *amplitudeFrame = new QFrame();
  bottomTopLayout->addWidget(amplitudeFrame);
  amplitudeFrame->setFrameStyle(QFrame::WinPanel | QFrame::Sunken);
  QVBoxLayout *amplitudeFrameLayout = new QVBoxLayout();
  amplitudeFrame->setLayout(amplitudeFrameLayout);
  amplitudeWidget = new AmplitudeWidget(NULL);
  amplitudeFrameLayout->addWidget(amplitudeWidget);
  amplitudeWidget->setWhatsThis("Shows the volume (or other parameters) at time lined up with the pitch above. Note: You can move the lines to change some thresholds.");
  amplitudeFrameLayout->setMargin(0);
  amplitudeFrameLayout->setSpacing(0);

  QBoxLayout *bottomTopRightLayout = new QVBoxLayout();
  bottomTopLayout->addLayout(bottomTopRightLayout);
  
  amplitudeWheelY = new QwtWheel();
  bottomTopRightLayout->addWidget(amplitudeWheelY, 0);
  amplitudeWheelY->setOrientation(Qt::Vertical);
  amplitudeWheelY->setWheelWidth(14);
  amplitudeWheelY->setRange(0.2, 1.00, 0.01, 1);
  amplitudeWheelY->setValue(amplitudeWidget->range());
  amplitudeWheelY->setToolTip("Zoom pitch contour view vertically");
  
  //Create the vertical scrollbar
  amplitudeScrollBar = new MyScrollBar(0.0, 1.0-amplitudeWidget->range(), 0.20, amplitudeWidget->range(), 0, 20, Qt::Vertical, bottomWidget);
  bottomTopRightLayout->addWidget(amplitudeScrollBar, 4);
  
  QBoxLayout *bottomBottomLayout = new QHBoxLayout();
  bottomLayout->addLayout(bottomBottomLayout);

  bottomBottomLayout->addStretch(2);
  QComboBox *amplitudeModeComboBox = new QComboBox();
  bottomLayout->addWidget(amplitudeModeComboBox, 0);
  amplitudeModeComboBox->setWhatsThis("Select different algorithm parameters to view in the bottom pannel");
  int j;
  QStringList s;
  for(j=0; j<NUM_AMP_MODES; j++) s << amp_mode_names[j];
  amplitudeModeComboBox->addItems(s);
  connect(amplitudeModeComboBox, SIGNAL(activated(int)), gdata, SLOT(setAmplitudeMode(int)));
  connect(amplitudeModeComboBox, SIGNAL(activated(int)), amplitudeWidget, SLOT(update()));

  bottomBottomLayout->addStretch(2);
  QComboBox *pitchContourModeComboBox = new QComboBox();
  bottomLayout->addWidget(pitchContourModeComboBox, 0);
  pitchContourModeComboBox->setWhatsThis("Select whether the Pitch Contour line fades in/out with clarity/loudness of the sound or is a solid dark line");
  s.clear();
  s << "Clarity fading" << "Note grouping";
  pitchContourModeComboBox->addItems(s);
  connect(pitchContourModeComboBox, SIGNAL(activated(int)), gdata, SLOT(setPitchContourMode(int)));
  connect(pitchContourModeComboBox, SIGNAL(activated(int)), freqWidgetGL, SLOT(update()));

  bottomBottomLayout->addStretch(2);
  freqWheelX = new QwtWheel();
  bottomLayout->addWidget(freqWheelX, 1);
  freqWheelX->setOrientation(Qt::Horizontal);
  freqWheelX->setWheelWidth(16);
  freqWheelX->setRange(0.5, 9.0, 0.001, 1);
  freqWheelX->setValue(2.0);
  freqWheelX->setToolTip("Zoom horizontally");

  bottomBottomLayout->addSpacing(16);

  //Create the resize grip. The thing in the bottom right hand corner
  QSizeGrip *sizeGrip = new QSizeGrip(bottomWidget);
  bottomLayout->addWidget(sizeGrip);
  sizeGrip->setFixedSize(15, 15);
  QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  policy.setHeightForWidth(false);
  sizeGrip->setSizePolicy(policy);
  bottomBottomLayout->addWidget(sizeGrip);
      
  //Setup all the signals and slots
  //vertical

  connect(freqScrollBar, SIGNAL(sliderMoved(double)), view, SLOT(changeViewY(double)));
  connect(freqScrollBar, SIGNAL(sliderMoved(double)), view, SLOT(doSlowUpdate()));

  connect(view, SIGNAL(viewBottomChanged(double)), freqScrollBar, SLOT(setValue(double)));
  connect(freqWheelY, SIGNAL(valueChanged(double)), view, SLOT(setZoomFactorY(double)));
  connect(view, SIGNAL(logZoomYChanged(double)), freqWheelY, SLOT(setValue(double)));
  
  //horizontal
  connect(freqWheelX, SIGNAL(valueChanged(double)), view, SLOT(setZoomFactorX(double)));
  connect(view, SIGNAL(logZoomXChanged(double)), freqWheelX, SLOT(setValue(double)));
  connect(amplitudeWheelY, SIGNAL(valueChanged(double)), amplitudeWidget, SLOT(setRange(double)));
  connect(amplitudeWheelY, SIGNAL(valueChanged(double)), amplitudeWidget, SLOT(update()));

  connect(amplitudeScrollBar, SIGNAL(sliderMoved(double)), amplitudeWidget, SLOT(setOffset(double)));
  connect(amplitudeScrollBar, SIGNAL(sliderMoved(double)), amplitudeWidget, SLOT(update()));

  connect(amplitudeWidget, SIGNAL(rangeChanged(double)), this, SLOT(setAmplitudeZoom(double)));
  connect(amplitudeWidget, SIGNAL(rangeChanged(double)), amplitudeWheelY, SLOT(setValue(double)));
  connect(amplitudeWidget, SIGNAL(offsetChanged(double)), amplitudeScrollBar, SLOT(setValue(double)));

  //make the widgets get updated when the view changes
  connect(gdata->view, SIGNAL(onSlowUpdate(double)), freqWidgetGL, SLOT(update()));
  connect(gdata->view, SIGNAL(onSlowUpdate(double)), amplitudeWidget, SLOT(update()));
  connect(gdata->view, SIGNAL(onSlowUpdate(double)), timeAxis, SLOT(update()));
  connect(gdata->view, SIGNAL(timeViewRangeChanged(double, double)), timeAxis, SLOT(setRange(double, double)));
}

FreqView::~FreqView()
{
  //Qt deletes the child widgets automatically
}

void FreqView::zoomIn()
{
  bool doneIt = false;
  if(gdata->running != STREAM_FORWARD) {
    if(freqWidgetGL->testAttribute(Qt::WA_UnderMouse)) {
      QPoint mousePos = freqWidgetGL->mapFromGlobal(QCursor::pos());
      gdata->view->setZoomFactorX(gdata->view->logZoomX() + 0.1, mousePos.x());
      gdata->view->setZoomFactorY(gdata->view->logZoomY() + 0.1, freqWidgetGL->height() - mousePos.y());
      doneIt = true;
    } else if(amplitudeWidget->testAttribute(Qt::WA_UnderMouse)) {
      QPoint mousePos = amplitudeWidget->mapFromGlobal(QCursor::pos());
      gdata->view->setZoomFactorX(gdata->view->logZoomX() + 0.1, mousePos.x());
      doneIt = true;
    }
  }
  if(!doneIt) {
    gdata->view->setZoomFactorX(gdata->view->logZoomX() + 0.1);
    gdata->view->setZoomFactorY(gdata->view->logZoomY() + 0.1);
    doneIt = true;
  }
}

void FreqView::zoomOut()
{
  gdata->view->setZoomFactorX(gdata->view->logZoomX() - 0.1);
  if(!amplitudeWidget->testAttribute(Qt::WA_UnderMouse)) {
    gdata->view->setZoomFactorY(gdata->view->logZoomY() - 0.1);
  }
}

void FreqView::setAmplitudeZoom(double newRange)
{
  amplitudeScrollBar->setRange(0.0, 1.0-newRange);
  amplitudeScrollBar->setPageStep(newRange);
}
