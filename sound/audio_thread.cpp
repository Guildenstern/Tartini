/***************************************************************************
                          audio_thread.cpp  -  description
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

#include "audio_thread.h"
#include "soundfile.h"
#include "gdata.h"
#include "mainwindow.h"
#include "audio_stream.h"
#include <QtGui/QApplication>
#include <cstdio>

const QEvent::Type AudioThread::UPDATE_FAST
    = static_cast<QEvent::Type>(QEvent::registerEventType());
const QEvent::Type AudioThread::UPDATE_SLOW
= static_cast<QEvent::Type>(QEvent::registerEventType());
const QEvent::Type AudioThread::SOUND_STARTED
= static_cast<QEvent::Type>(QEvent::registerEventType());
const QEvent::Type AudioThread::SOUND_STOPPED
= static_cast<QEvent::Type>(QEvent::registerEventType());
const QEvent::Type AudioThread::SETTINGS_CHANGED
= static_cast<QEvent::Type>(QEvent::registerEventType());

AudioThread::AudioThread() {
  //printf("Warning - audio thread created with no sound file specified.\n");
  _playSoundFile = NULL;
  _recSoundFile = NULL;
  sleepCount = 0;
}

void AudioThread::start() {
  fprintf(stderr, "Warning - audio thread created with no sound file specified.\n");
  _playSoundFile = NULL;
  _recSoundFile = NULL;
  stopping = false;
  QThread::start(QThread::HighPriority/*TimeCriticalPriority*/);
}

#define S(s) s.toLocal8Bit().data()

void AudioThread::start(SoundFile *sPlay, SoundFile *sRec) {
  fprintf(stderr, "Audio thread created for\n");
  if(sPlay) {
      fprintf(stderr, "Playing file %s\n", S(sPlay->filename));
      fflush(stdout);
  }
  if(sRec) {
      fprintf(stderr, "Recording file %s\n", S(sRec->filename));
      fflush(stdout);
  }
  _playSoundFile = sPlay;
  _recSoundFile = sRec;
  stopping = false;
  QThread::start(QThread::HighPriority/*TimeCriticalPriority*/);
}

/** Causes the audio thread to stop at the end of the next loop
*/
void AudioThread::stop()
{
  stopping = true;
}

/** Stop the audio thread and waits for it to finish
*/
void AudioThread::stopAndWait()
{
  stop();
  wait();
}

void AudioThread::run()
{
#ifndef WINDOWS
  //setup stuff for multi-threaded profiling
  profiler_ovalue.it_interval.tv_sec = 0;
  profiler_ovalue.it_interval.tv_usec = 0;
  profiler_ovalue.it_value.tv_sec = 0;
  profiler_ovalue.it_value.tv_usec = 0;
  setitimer(ITIMER_PROF, &profiler_value, &profiler_ovalue); //for running multi-threaded profiling
#endif

  first = true;
  fast_update_count = 0;
  slow_update_count = 0;
  frame_num = 0;

  //read to the 1 chunk befor time 0
  if((gdata->soundMode & SOUND_REC)) {
    gdata->setDoingActiveAnalysis(true);
    myassert(_recSoundFile->firstTimeThrough == true);
    _recSoundFile->recordChunk(_recSoundFile->offset());
  }
  
  QApplication::postEvent(gdata->mainWindow, new QEvent(SOUND_STARTED));
  gdata->running = STREAM_FORWARD;

  while(!stopping) {
    if(doStuff() == 0) break;
//#ifdef _OS_WIN32_
#ifdef WINDOWS
    Sleep(0); //make other threads do some stuff
#else
    sleep(0); //make other threads do some stuff
    //usleep(1);
    //qApp->wakeUpGuiThread();
    //qApp->processEvents();
#endif
  }

  gdata->running = STREAM_STOP;

  if((gdata->soundMode & SOUND_REC)) {
    gdata->setDoingActiveAnalysis(false);
    _recSoundFile->firstTimeThrough = false;
   _recSoundFile->rec2play();
    gdata->soundMode = SOUND_PLAY;
  }

  if(gdata->audio_stream) {
    delete gdata->audio_stream;
    gdata->audio_stream = NULL;
  }
  _playSoundFile = NULL;
  _recSoundFile = NULL;
  
  QApplication::postEvent(gdata->mainWindow, new QEvent(SOUND_STOPPED));
}

int AudioThread::doStuff()
{
  int force_update = false;
  if(gdata->running == STREAM_PAUSE) { msleep(20); return 1; } //paused
  if(!_playSoundFile && !_recSoundFile) return 0;

  ++frame_num;
	
  //update one frame before pausing again
  if(gdata->running == STREAM_UPDATE) {
    gdata->running = STREAM_PAUSE; //update then pause
    force_update = true;
  }
  if(gdata->running != STREAM_FORWARD) return 0;
  
  //This is the main block of code for reading or write the next chunk to the soundcard or file
//#if WINDOWS
//  if((gdata->soundMode & SOUND_PLAY)) {
//#else
  if(gdata->soundMode == SOUND_PLAY) {
//#endif
	  if(!_playSoundFile->playChunk()) { //end of file
      QApplication::postEvent( gdata->mainWindow, new QEvent(UPDATE_SLOW)); //for qt3.x
      return 0; //stop the audio thread playing
    }
    if(!gdata->audio_stream) {
      if (++sleepCount % 4 == 0) {
        int sleepval = (1000 * _playSoundFile->framesPerChunk()) / _playSoundFile->rate();
        msleep(sleepval * 4);
      }
    }
  } else if(gdata->soundMode == SOUND_REC) {  // SOUND_REC
    int bufferChunks = gdata->audio_stream->inTotalBufferFrames() / _recSoundFile->framesPerChunk();
    if(frame_num > bufferChunks)
      _recSoundFile->recordChunk(_recSoundFile->framesPerChunk());
	//msleep(1);
  }
  else if(gdata->soundMode == SOUND_PLAY_REC) {
    int bufferChunks = gdata->audio_stream->inTotalBufferFrames() / _recSoundFile->framesPerChunk();
    if(frame_num > bufferChunks) {
      if(!SoundFile::playRecordChunk(_playSoundFile, _recSoundFile)) { //end of file
        QApplication::postEvent( gdata->mainWindow, new QEvent(UPDATE_SLOW)); //for qt3.x
        return 0; //stop the audio thread playing
      }
    }
  }

  //Set some flags to cause an update of views, every now and then
  fast_update_count++;
  slow_update_count++;
  int slowUpdateAfter = toInt(double(gdata->slowUpdateSpeed()) / 1000.0 / curSoundFile()->timePerChunk());
  int fastUpdateAfter = toInt(double(gdata->fastUpdateSpeed()) / 1000.0 / curSoundFile()->timePerChunk());
  if(!gdata->need_update) {
    if((slow_update_count >= slowUpdateAfter) || force_update) {
      gdata->need_update = true;
      fast_update_count = 0;
      slow_update_count = 0;
      QApplication::postEvent(gdata->mainWindow, new QEvent(UPDATE_SLOW));
    } else if(fast_update_count >= fastUpdateAfter) {
      gdata->need_update = true;
      fast_update_count = 0;
      QApplication::postEvent(gdata->mainWindow, new QEvent(UPDATE_FAST));
    }
  }
  gdata->doChunkUpdate();

  return 1;
}
