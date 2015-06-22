/***************************************************************************
                          openfiles.cpp  -  description
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

#include "openfiles.h"
#include "gdata.h"
#include "channel.h"
#include "view.h"
#include "analysisdata.h"
#include "notedata.h"
#include <QtGui/QTreeWidget>

OpenFiles::OpenFiles(int id, QWidget *parent) : ViewWidget(id, parent)
{
  //setCaption("Open Files");

  //Create the list of channels down the left hand side
  theListView = new QTreeWidget(this);
  theListView->setColumnCount(2);
  theListView->setHeaderLabels((QStringList() << "Filename (Channel)" << "A"));
  //theListView->addColumn("Filename (Channel)", 178);
  //theListView->addColumn("A", 20);

  theListView->setWhatsThis("A list of all open channels in all open sounds. "
    "The current active channel is marked with an 'A' beside it. "
    "The tick specifies if a channel should be visible or not in the multi-channel views");
  // Make it so the Active column magically appears if needed
  ///theListView->setSelectionMode(QTreeWidget::Extended);
  ///theListView->setSelectionMode(QTreeWidget::Single);
  ///theListView->setSorting(-1);
  theListView->setFocusPolicy(Qt::NoFocus);

  connect(gdata, SIGNAL(channelsChanged()), this, SLOT(refreshChannelList()));
  connect(gdata, SIGNAL(activeChannelChanged(Channel*)), this, SLOT(slotActiveChannelChanged(Channel *)));
  connect(theListView, SIGNAL(itemPressed(QTreeWidgetItem*,int)), this, SLOT(listViewChanged(QTreeWidgetItem*,int)));
  connect(theListView, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), this, SLOT(slotCurrentChanged(QTreeWidgetItem*,QTreeWidgetItem*)));

  refreshChannelList();
}

OpenFiles::~OpenFiles()
{
}
  
void OpenFiles::refreshChannelList()
{
  //put in any channel items that already exist
  //char s[2048];
  theListView->clear();

  int j=0;
  QTreeWidgetItem* root = theListView->invisibleRootItem();
  for(std::vector<Channel*>::iterator it = gdata->channels.begin(); it != gdata->channels.end(); it++) {
    QStringList s;
    s << (*it)->getUniqueFilename();
    QTreeWidgetItem *newElement = new QTreeWidgetItem(root, s);
    
    if((*it)->isVisible()) {
      //newElement->setOn(true);
    }
    if((*it) == gdata->getActiveChannel()) {
      newElement->setText(1, "A");
      newElement->setSelected(true);
      theListView->setCurrentItem(newElement);
    }
    j++;
  }
}

//TODO: Tidy this method up
void OpenFiles::slotActiveChannelChanged(Channel *active)
{
  int index = 0;
	bool found = false;

	// Find the index of the active channel
	for (index = 0; index < int(gdata->channels.size()); index++) {
		if (gdata->channels.at(index) == active) {
			found = true;
			break;
		}
	}

	// Set the active marker for each item on or off, depending on what it should be.
	// This depends on them being in the same order as the channels list.
	if (found) {
		int pos = 0;
		// Go through all the elements in the list view and turn the active channel 
		// markers off, or on if we find the right index

                QTreeWidgetItem* root = theListView->invisibleRootItem();
                while (root->childCount() > pos) {
                    QTreeWidgetItem *item = root->child(pos);
			if (pos == index) {
                            item->setSelected(true);
			//  item->setText(1, "A");
			//} else {
			//	item->setText(1, "");
			}
                        pos++;
		}
        }
}

/**
 * Toggles a channel on or off for a specified item.
 *
 * @param item the channel to toggle.
 **/
void OpenFiles::listViewChanged(QTreeWidgetItem* item, int /*column*/)
{
  if(item == NULL) return;
  int pos = 0;
  QTreeWidgetItem* root = theListView->invisibleRootItem();
  while (root->childCount() > pos) {
    QTreeWidgetItem *myChild = root->child(pos);
    if(myChild == item) break;
    pos++;
  }
  myassert(pos < int(gdata->channels.size()));
  bool state = item->isSelected();
  if(gdata->channels.at(pos)->isVisible() != state) gdata->channels.at(pos)->setVisible(state);
  gdata->view->doUpdate();
}

/**
 * Changes the active channel to the item.
 *
 * @param item the channel to toggle.
 **/
void OpenFiles::slotCurrentChanged(QTreeWidgetItem* item, QTreeWidgetItem* prev)
{
  if(item == NULL) return;
  int pos = 0;
  // Go through the channels before the active one, and reset the markers
  QTreeWidgetItem* root = theListView->invisibleRootItem();
  QTreeWidgetItem *myChild;
  while (root->childCount() > pos) {
    myChild = root->child(pos);
    if(myChild == item) break;
    myChild->setText(1, "");
    pos++;
  }
  myassert(pos < int(gdata->channels.size()));
  myChild->setText(1, "A");
  gdata->setActiveChannel(gdata->channels.at(pos));

  // Go through the rest of the items and reset their active channel markers
  while (root->childCount() > pos) {
    myChild = root->child(pos);
    if(myChild == item) break;
    myChild->setText(1, "");
    pos++;
  }
}

void OpenFiles::resizeEvent(QResizeEvent *)
{
  theListView->resize(size());
}
