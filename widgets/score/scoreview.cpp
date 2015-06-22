/***************************************************************************
                          scoreview.cpp  -  description
                             -------------------
    begin                : Mar 26 2006
    copyright            : (C) 2006 by Philip McLeod
    email                : pmcleod@cs.otago.ac.nz
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
 ***************************************************************************/

#include "scoreview.h"
#include "scorewidget.h"
#include "gdata.h"
#include "view.h"
#include <qwt/qwt_wheel.h>
#include <QtGui/QBoxLayout>
#include <QtGui/QToolTip>
#include <QtGui/QComboBox>

ScoreView::ScoreView( int viewID_, QWidget *parent )
 : ViewWidget( viewID_, parent)
{

  QBoxLayout *mainLayout = new QHBoxLayout();
  QBoxLayout *leftLayout = new QVBoxLayout();
  mainLayout->addLayout(leftLayout);
  QBoxLayout *rightLayout = new QVBoxLayout();
  mainLayout->addLayout(rightLayout);

  //create a drawing object
  scoreWidget = new ScoreWidget(this);
  leftLayout->addWidget(scoreWidget);
  QBoxLayout *bottomLayout = new QHBoxLayout();
  leftLayout->addLayout(bottomLayout);

  //vertical scroll wheel
  QwtWheel *scaleWheelY = new QwtWheel();
  scaleWheelY->setOrientation(Qt::Vertical);
  scaleWheelY->setWheelWidth(14);
  scaleWheelY->setRange(1.0, 30.0, 0.1, 1);
  scaleWheelY->setValue(scoreWidget->scaleY());
  scaleWheelY->setToolTip("Zoom vertically");
  rightLayout->addWidget(scaleWheelY, 1);
  rightLayout->addStretch(4);
  connect(scaleWheelY, SIGNAL(sliderMoved(double)), scoreWidget, SLOT(setScaleY(double)));
  connect(scaleWheelY, SIGNAL(sliderMoved(double)), scoreWidget, SLOT(update()));

  //horizontal scroll wheel
  QwtWheel* scaleWheelX = new QwtWheel(this);
  scaleWheelX->setOrientation(Qt::Horizontal);
  scaleWheelX->setWheelWidth(16);
  scaleWheelX->setRange(1.0, 100.0, 1.1, 1);
  scaleWheelX->setValue(scoreWidget->scaleX());
  scaleWheelX->setToolTip("Zoom horizontally");
  connect(scaleWheelX, SIGNAL(sliderMoved(double)), scoreWidget, SLOT(setScaleX(double)));
  connect(scaleWheelX, SIGNAL(sliderMoved(double)), scoreWidget, SLOT(update()));

  QComboBox *sharpsModeComboBox = new QComboBox(this);
  QStringList s;
  s << "Sharps" << "Flats";
  sharpsModeComboBox->addItems(s);
  connect(sharpsModeComboBox, SIGNAL(activated(int)), scoreWidget, SLOT(setSharpsMode(int)));
  connect(sharpsModeComboBox, SIGNAL(activated(int)), scoreWidget, SLOT(update()));

  QComboBox *notesModeComboBox = new QComboBox(this);
  s.clear();
  s << "Notes" << "MIDI blocks";
  notesModeComboBox->addItems(s);
  connect(notesModeComboBox, SIGNAL(activated(int)), scoreWidget, SLOT(setNotesMode(int)));
  connect(notesModeComboBox, SIGNAL(activated(int)), scoreWidget, SLOT(update()));

  QComboBox *clefModeComboBox = new QComboBox(this);
  s.clear();
  s << "Both clefs" << "Treble clef";
  clefModeComboBox->addItems(s);
  connect(clefModeComboBox, SIGNAL(activated(int)), scoreWidget, SLOT(setClefMode(int)));
  connect(clefModeComboBox, SIGNAL(activated(int)), scoreWidget, SLOT(update()));

  QComboBox *opaqueModeComboBox = new QComboBox(this);
  s.clear();
  s << "See-through notes" << "Opaque notes";
  opaqueModeComboBox->addItems(s);
  connect(opaqueModeComboBox, SIGNAL(activated(int)), scoreWidget, SLOT(setOpaqueMode(int)));
  connect(opaqueModeComboBox, SIGNAL(activated(int)), scoreWidget, SLOT(update()));

  QComboBox *transposeComboBox = new QComboBox(this);
  s.clear();
  s << "Up 2 octaves" << "Up 1 octave" << "As played" << "Down 1 octave" << "Down 2 octaves";
  transposeComboBox->addItems(s);
  transposeComboBox->setCurrentIndex(2);
  connect(transposeComboBox, SIGNAL(activated(int)), scoreWidget, SLOT(setTransposeLevel(int)));
  connect(transposeComboBox, SIGNAL(activated(int)), scoreWidget, SLOT(update()));

  QComboBox *showAllModeComboBox = new QComboBox(this);
  s.clear();
  s << "Hide extream notes" << "Show all notes";
  showAllModeComboBox->addItems(s);
  connect(showAllModeComboBox, SIGNAL(activated(int)), scoreWidget, SLOT(setShowAllMode(int)));
  connect(showAllModeComboBox, SIGNAL(activated(int)), scoreWidget, SLOT(update()));

  bottomLayout->addWidget(scaleWheelX, 0);
  bottomLayout->addStretch(1);
  bottomLayout->addWidget(sharpsModeComboBox, 0);
  bottomLayout->addStretch(1);
  bottomLayout->addWidget(notesModeComboBox, 0);
  bottomLayout->addStretch(1);
  bottomLayout->addWidget(clefModeComboBox, 0);
  bottomLayout->addStretch(1);
  bottomLayout->addWidget(opaqueModeComboBox, 0);
  bottomLayout->addStretch(1);
  bottomLayout->addWidget(transposeComboBox, 0);
  bottomLayout->addStretch(1);
  bottomLayout->addWidget(showAllModeComboBox, 0);
  bottomLayout->addStretch(4);

  setLayout(mainLayout);

  //make any connections
  connect(gdata->view, SIGNAL(onFastUpdate(double)), scoreWidget, SLOT(update()));
}

ScoreView::~ScoreView()
{
  delete scoreWidget;
}

void ScoreView::resizeEvent(QResizeEvent *)
{
  //scoreWidget->resize(size());
}
