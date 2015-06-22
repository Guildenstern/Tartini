/***************************************************************************
                          audio_thread.h  -  description
                             -------------------
    begin                : 2002
    copyright            : (C) 2002-2005 by Philip McLeod
    email                : pmcleod@cs.otago.ac.nz
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
 ***************************************************************************/
#ifndef AUDIO_THREAD_H
#define AUDIO_THREAD_H

#include <QtCore/QThread>
#include <QtCore/QEvent>

#ifdef _OS_WIN32_
#include <windows.h>
#endif

class SoundFile;

class AudioThread : public QThread {
public:
    static const QEvent::Type UPDATE_FAST;
    static const QEvent::Type UPDATE_SLOW;
    static const QEvent::Type SOUND_STARTED;
    static const QEvent::Type SOUND_STOPPED;
    static const QEvent::Type SETTINGS_CHANGED;
    AudioThread();
  virtual ~AudioThread() {}

  virtual void run();
  void start();
  void start(SoundFile *sPlay, SoundFile *sRec);
  void stop();
  void stopAndWait();
      
  int doStuff();
  SoundFile *playSoundFile() { return _playSoundFile; }
  SoundFile *recSoundFile() { return _recSoundFile; }
  //SoundFile *curSoundFile() { return (_playSoundFile) ? _playSoundFile : _recSoundFile; }
  SoundFile *curSoundFile() { return (_recSoundFile) ? _recSoundFile : _playSoundFile; }
  
 private:
  SoundFile *_playSoundFile;
  SoundFile *_recSoundFile;
  
  bool stopping;
  bool first;
  int fast_update_count;
  int slow_update_count;
  int frame_num;

  bool useFile;
  //FILE *freqFile;

   int sleepCount;
};

#endif
