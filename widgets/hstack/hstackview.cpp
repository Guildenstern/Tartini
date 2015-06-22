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
#include "hstackview.h"
#include "notedata.h"
#include "hstackwidget.h"
#include <qwt/qwt_wheel.h>
#include <QtGui/QBoxLayout>
#include <QtGui/QGridLayout>
#include <QtGui/QToolTip>

HStackView::HStackView( int viewID_, QWidget *parent )
 : ViewWidget( viewID_, parent)
{
  setWindowTitle("Harmonic Stack");

  QBoxLayout *mainLayout = new QHBoxLayout(this);

  QBoxLayout *leftLayout = new QVBoxLayout();
  mainLayout->addItem(leftLayout);
  QBoxLayout *rightLayout = new QVBoxLayout();
  mainLayout->addItem(rightLayout);

  hStackWidget = new HStackWidget(this);
  leftLayout->addWidget(hStackWidget);
  
  QBoxLayout *bottomLayout = new QHBoxLayout();
  leftLayout->addItem(bottomLayout);

  QwtWheel* dbRangeWheel = new QwtWheel(this);
  dbRangeWheel->setOrientation(Qt::Vertical);
  dbRangeWheel->setWheelWidth(14);
  dbRangeWheel->setRange(5, 160.0, 0.1, 100);
  dbRangeWheel->setValue(100);
  dbRangeWheel->setToolTip("Zoom dB range vertically");
  rightLayout->addWidget(dbRangeWheel, 0);
  rightLayout->addStretch(2);

  QwtWheel* windowSizeWheel = new QwtWheel(this);
  windowSizeWheel->setOrientation(Qt::Horizontal);
  windowSizeWheel->setWheelWidth(14);
  windowSizeWheel->setRange(32, 1024, 2, 1);
  windowSizeWheel->setValue(128);
  windowSizeWheel->setToolTip("Zoom windowsize horizontally");
  bottomLayout->addWidget(windowSizeWheel, 0);
  bottomLayout->addStretch(2); 

  connect(dbRangeWheel, SIGNAL(valueChanged(double)), hStackWidget, SLOT(setDBRange(double)));
  connect(dbRangeWheel, SIGNAL(valueChanged(double)), hStackWidget, SLOT(update()));

  connect(windowSizeWheel, SIGNAL(valueChanged(double)), hStackWidget, SLOT(setWindowSize(double)));
  connect(windowSizeWheel, SIGNAL(valueChanged(double)), hStackWidget, SLOT(update()));
}

HStackView::~HStackView()
{
  delete hStackWidget;
}
