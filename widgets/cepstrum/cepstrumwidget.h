/***************************************************************************
                          cepstrumwidget.h  -  description
                             -------------------
    begin                : May 21 2005
    copyright            : (C) 2005 by Philip McLeod
    email                : pmcleod@cs.otago.ac.nz
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
 ***************************************************************************/
#ifndef CEPSTRUMWIDGET_H
#define CEPSTRUMWIDGET_H

#include "drawwidget.h"

class CepstrumWidget : public DrawWidget {
  Q_OBJECT

  public:
    CepstrumWidget(QWidget *parent);
    virtual ~CepstrumWidget();

    void paintEvent( QPaintEvent * );

    QSize sizeHint() const { return QSize(500, 128); }

  private:
    QPolygon pointArray;
};


#endif
