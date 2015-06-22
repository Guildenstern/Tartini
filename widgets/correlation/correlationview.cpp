/***************************************************************************
                          correlationview.cpp  -  description
                             -------------------
    begin                : May 2 2005
    copyright            : (C) 2005 by Philip McLeod
    email                : pmcleod@cs.otago.ac.nz
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
 ***************************************************************************/
#include "correlationview.h"
#include "correlationwidget.h"
#include "gdata.h"
#include "view.h"
#include "channel.h"
#include "notedata.h"
#include "analysisdata.h"
#include <QtGui/QComboBox>
#include <QtGui/QHBoxLayout>

CorrelationView::CorrelationView( int viewID_, QWidget *parent )
 : ViewWidget( viewID_, parent)
{
  //setCaption("Wave view");
  gdata->setDoingActiveAnalysis(true);

  Channel *active = gdata->getActiveChannel();
  if(active) {
    active->lock();
    active->processChunk(active->currentChunk());
    active->unlock();
  }

  correlationWidget = new CorrelationWidget(this);
  QStringList s;
  s << "Chunk correlation" << "Note Aggregate Correlation" << "Note Aggregate Correlation Scaled";
  QComboBox *aggregateModeComboBox = new QComboBox(this);
  aggregateModeComboBox->addItems(s);
  QHBoxLayout *hLayout = new QHBoxLayout();
  hLayout->setMargin(0);
  hLayout->addWidget(aggregateModeComboBox);
  hLayout->addStretch(1);
  connect(aggregateModeComboBox, SIGNAL(activated(int)), correlationWidget, SLOT(setAggregateMode(int)));

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setMargin(0);
  mainLayout->addWidget(correlationWidget);
  mainLayout->addLayout(hLayout);


  //make the widget get updated when the view changes
  //connect(gdata->view, SIGNAL(onFastUpdate(double)), correlationWidget, SLOT(update()));
  connect(gdata->view, SIGNAL(onSlowUpdate(double)), correlationWidget, SLOT(update()));
}

CorrelationView::~CorrelationView()
{
  gdata->setDoingActiveAnalysis(false);
  delete correlationWidget;
}

/*
void CorrelationView::resizeEvent(QResizeEvent *)
{
  correlationWidget->resize(size());
}
*/
