/***************************************************************************
                          waveview.cpp  -  description
                             -------------------
    begin                : Mon Mar 14 2005
    copyright            : (C) 2005 by Philip McLeod
    email                : pmcleod@cs.otago.ac.nz
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
 ***************************************************************************/
#include "waveview.h"
#include "wavewidget.h"
#include "gdata.h"
#include "view.h"
#include <qwt/qwt_wheel.h>
#include <QtGui/QBoxLayout>
#include <QtGui/QToolTip>

WaveView::WaveView( int viewID_, QWidget *parent )
 : ViewWidget( viewID_, parent)
{
  //setCaption("Wave view");

  QBoxLayout *mainLayout = new QHBoxLayout();
  setLayout(mainLayout);

  waveWidget = new WaveWidget(this);
  mainLayout->addWidget(waveWidget);

  QBoxLayout *rightLayout = new QVBoxLayout();
  mainLayout->addLayout(rightLayout);
  
  QwtWheel *freqWheelY = new QwtWheel();
  rightLayout->addWidget(freqWheelY, 1);
  freqWheelY->setOrientation(Qt::Vertical);
  freqWheelY->setWheelWidth(14);
  freqWheelY->setRange(1.0, 20.0, 0.1, 1);
  freqWheelY->setValue(1.0);
  freqWheelY->setToolTip("Zoom oscilloscope vertically");
  rightLayout->addStretch(2);
  
  connect(freqWheelY, SIGNAL(valueChanged(double)), waveWidget, SLOT(setZoomY(double)));
  connect(waveWidget, SIGNAL(zoomYChanged(double)), waveWidget, SLOT(update()));
  
  //make the widget get updated when the view changes
  connect(gdata->view, SIGNAL(onSlowUpdate(double)), waveWidget, SLOT(update()));
}

WaveView::~WaveView()
{
}

void WaveView::resizeEvent(QResizeEvent *)
{
  //waveWidget->resize(size());
}
