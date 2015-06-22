/***************************************************************************
                          channel.h  -  description
                             -------------------
    begin                : Sat Jul 10 2004
    copyright            : (C) 2004-2005 by Philip McLeod
    email                : pmcleod@cs.otago.ac.nz
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
 ***************************************************************************/

#ifndef CHANNEL_H
#define CHANNEL_H

#include "channelbase.h"
#include "zoomlookup.h"
#include "soundfile.h"

class Channel : public ChannelBase
{
private:
  SoundFile *parent;
  float freq; /**< Channel's frequency */
  int _pitch_method;
  bool visible;
  QMutex *mutex;
  bool isLocked;

  void setParent(SoundFile *parent_) { parent = parent_; }
  float *begin() { return directInput.begin(); }
public:
  QColor color;
  Filter *highPassFilter;
  
  ZoomLookup summaryZoomLookup;
  ZoomLookup normalZoomLookup;
  ZoomLookup amplitudeZoomLookup;
  
  void lock() { mutex->lock(); isLocked = true; }
  void unlock() { isLocked = false; mutex->unlock(); }
  bool locked() { return isLocked; } //For same thread testing of asserts only

  Channel(SoundFile *parent_, int size_, int k_=0);
  virtual ~Channel();
  float *end() { return directInput.end(); }
  void shift_left(int n);
  SoundFile* getParent() { return parent; }
  void processNewChunk();
  void processChunk(int chunk);
  bool isVisible() { return visible; }
  void setVisible(bool state=true) { visible = state; }
  void reset();
  double timePerChunk() { return parent->timePerChunk(); }
  double startTime() { return parent->startTime(); }
  double finishTime() { return startTime() + totalTime(); }
  double totalTime() { return double(MAX(totalChunks()-1, 0)) * timePerChunk(); }
  void jumpToTime(double t) { parent->jumpToTime(t); }
  int chunkAtTime(double t) { return parent->chunkAtTime(t); }
  double chunkFractionAtTime(double t) { return parent->chunkFractionAtTime(t); }
  int currentChunk() { return parent->currentChunk(); } //this one should be use to retrieve current info
  double timeAtChunk(int chunk) { return parent->timeAtChunk(chunk); }

  AnalysisData *dataAtTime(double t) { return dataAtChunk(chunkAtTime(t)); }
  std::deque<AnalysisData>::iterator dataIteratorAtChunk(int chunk) { return lookup.begin() + chunk; }

  bool hasAnalysisData() { return !lookup.empty(); }
  float averagePitch(int begin, int end);

  void setIntThreshold(int thresholdPercentage) { _threshold = float(thresholdPercentage) / 100.0f; }
  void resetIntThreshold(int thresholdPercentage);
  void setColor(QColor c) { color = c; }

  bool isVisibleNote(int noteIndex_);
  bool isLabelNote(int noteIndex_);
  void clearFreqLookup();
  void clearAmplitudeLookup();
  void recalcScoreThresholds();

  QString getUniqueFilename();

  NoteData *getCurrentNote();
  bool firstTimeThrough() { return parent->firstTimeThrough; }
  bool doingDetailedPitch() { return parent->doingDetailedPitch(); }

  void exportChannel(int type, QString typeString);
  double dBFloor();
};

/** Create a ChannelLocker on the stack, the channel will be freed automaticly when
  the ChannelLocker goes out of scope
*/
class ChannelLocker
{
  Channel *channel;
  
public:
  ChannelLocker(Channel *channel_) {
    myassert(channel_);
    channel = channel_;
    channel->lock();
  }
  ~ChannelLocker() {
    channel->unlock();
  }
};

#endif
