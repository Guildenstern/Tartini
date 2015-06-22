/***************************************************************************
                          openfiles.h  -  description
                             -------------------
    begin                : Thu Dec 16 2004
    copyright            : (C) 2004-2005 by Philip McLeod
    email                : pmcleod@cs.otago.ac.nz
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
 ***************************************************************************/
#ifndef OPENFILES_H
#define OPENFILES_H

#include "viewwidget.h"

class QTreeWidget;
class QTreeWidgetItem;
class Channel;

class OpenFiles : public ViewWidget {
  Q_OBJECT

  private:
      QTreeWidget *theListView;

  public:
    OpenFiles(int id, QWidget *parent);
    virtual ~OpenFiles();

    void resizeEvent(QResizeEvent *);

    QSize sizeHint() const { return QSize(200, 300); }

  public slots:
    void refreshChannelList();
    void listViewChanged(QTreeWidgetItem* item, int column);
    void slotCurrentChanged(QTreeWidgetItem* item, QTreeWidgetItem* prev);
    void slotActiveChannelChanged(Channel *active);

};
#endif
