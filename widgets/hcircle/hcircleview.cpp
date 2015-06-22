/***************************************************************************
                          hstackview.cpp  -  description
                             -------------------
    begin                : Mon Jan 10 2005
    copyright            : (C) 2005 by Philip McLeod
    email                : pmcleod@cs.otago.ac.nz
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
 ***************************************************************************/
#include "hcircleview.h"
#include "hcirclewidget.h"
#include <qwt/qwt_wheel.h>
#include <QtGui/QBoxLayout>
#include <QtGui/QToolTip>

HCircleView::HCircleView( int viewID_, QWidget *parent )
 : ViewWidget( viewID_, parent)
{
  setWindowTitle("Harmonic Circle");

  QBoxLayout *mainLayout = new QHBoxLayout(this);
  QBoxLayout *leftLayout = new QVBoxLayout();
  mainLayout->addItem(leftLayout);
  QBoxLayout *rightLayout = new QVBoxLayout();
  mainLayout->addItem(rightLayout);

  hCircleWidget = new HCircleWidget(this);
  leftLayout->addWidget(hCircleWidget);

  QBoxLayout *bottomLayout = new QHBoxLayout();
  leftLayout->addItem(bottomLayout);
 
  QwtWheel* ZoomWheel = new QwtWheel(this);
  ZoomWheel->setOrientation(Qt::Vertical);
  ZoomWheel->setWheelWidth(14);
  ZoomWheel->setRange(0.001, 0.1, 0.001, 1);
  ZoomWheel->setValue(0.007);
  hCircleWidget->setZoom(0.007);
  ZoomWheel->setToolTip("Zoom in or out");
  rightLayout->addWidget(ZoomWheel);
  
  QwtWheel* lowestValueWheel = new QwtWheel(this);
  lowestValueWheel->setOrientation(Qt::Vertical);
  lowestValueWheel->setWheelWidth(14);
  lowestValueWheel->setRange(-160, 10, 0.01, 1);
  lowestValueWheel->setValue(-100);
  hCircleWidget->setLowestValue(-100);
  lowestValueWheel->setToolTip("Change the lowest value");
  rightLayout->addWidget(lowestValueWheel);
  rightLayout->addStretch(2);
 
  QwtWheel* thresholdWheel = new QwtWheel(this);
  thresholdWheel->setOrientation(Qt::Horizontal);
  thresholdWheel->setWheelWidth(14);
  thresholdWheel->setRange(-160, 10, 0.01, 1);
  thresholdWheel->setValue(-100);
  hCircleWidget->setThreshold(-100);
  thresholdWheel->setToolTip("Change the harmonic threshold");
  bottomLayout->addWidget(thresholdWheel);
  bottomLayout->addStretch(2);

  connect(ZoomWheel, SIGNAL(valueChanged(double)), hCircleWidget, SLOT(setZoom(double)));
  connect(ZoomWheel, SIGNAL(valueChanged(double)), hCircleWidget, SLOT(update()));

  connect(lowestValueWheel, SIGNAL(valueChanged(double)), hCircleWidget, SLOT(setLowestValue(double)));
  connect(lowestValueWheel, SIGNAL(valueChanged(double)), hCircleWidget, SLOT(update()));

  connect(thresholdWheel, SIGNAL(valueChanged(double)), hCircleWidget, SLOT(setThreshold(double)));
  connect(thresholdWheel, SIGNAL(valueChanged(double)), hCircleWidget, SLOT(update()));

}

HCircleView::~HCircleView()
{
  delete hCircleWidget;
}
