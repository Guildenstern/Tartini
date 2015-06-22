/***************************************************************************
                          main.h  -  description
                             -------------------
    begin                : May 2002
    copyright            : (C) 2002-2005 by Philip McLeod
    email                : pmcleod@cs.otago.ac.nz
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "mainwindow.h"

#include <QtGui/QApplication>

#ifdef MACX
#include <qstring.h>
extern QString macxPathString;
#endif
