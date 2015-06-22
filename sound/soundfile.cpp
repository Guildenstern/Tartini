 /***************************************************************************
                           soundfile.cpp  -  description
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

#include "soundfile.h"
#include "gdata.h"
#include "mystring.h"
#include "wave_stream.h"
#include "mainwindow.h"
#include "channel.h"
#include "audio_stream.h"
#include "analysisdata.h"
#include "notedata.h"
#include "Filter.h"
#include <QtGui/QProgressBar>
#include <QtGui/QMessageBox>
#include <QtGui/QApplication>
#include <QtGui/QStatusBar>
#include <QtGui/QLabel>
#include <QtCore/QSettings>
#include <QtCore/QDir>

#ifdef USE_SOX
#include "sox_stream.h"
#else
#if USE_OGG
#include "ogg_stream.h"
#endif
#endif

typedef unsigned char byte;

#define S(s) s.toLocal8Bit().data()

SoundFile::SoundFile()
{
  stream = NULL;
  filteredStream = NULL;
  _framesPerChunk = 0;
  tempWindowBuffer = NULL;
  tempWindowBufferDouble = NULL;
  tempWindowBufferFiltered = NULL;
  tempWindowBufferFilteredDouble = NULL;
  _startTime = 0.0;
  _chunkNum = 0;
  _offset = 0; //Number of frame to read into file to get to time 0 (half buffer size).
  _saved = true;
  mutex = new QMutex(QMutex::Recursive);
  firstTimeThrough = true;
  _doingDetailedPitch = false;
}

SoundFile::~SoundFile()
{
  uninit();
  delete mutex;
}

//free up all the memory of everything used
void SoundFile::uninit()
{
  if(gdata->audioThread.playSoundFile() == this || gdata->audioThread.recSoundFile() == this) {
    gdata->stop();
  }

  if(stream) { delete stream; stream = NULL; }
  if(filteredStream) {
    delete filteredStream; filteredStream = NULL;
    //Delete the temporary filtered file from disk!
    if(!QFile(filteredFilename).remove()) {
        fprintf(stderr, "Error removing file %s\n", S(filteredFilename));
    }
  }
  setFilename(QString());
  setFilteredFilename(QString());
  for(int j=0; j<numChannels(); j++) {
    delete channels(j);
    delete[] (tempWindowBuffer[j]-16);
    delete[] (tempWindowBufferDouble[j]-16);
    delete[] (tempWindowBufferFiltered[j]-16);
    delete[] (tempWindowBufferFilteredDouble[j]-16);
  }
  channels.resize(0);
  delete[] tempWindowBuffer; tempWindowBuffer = NULL;
  delete[] tempWindowBufferDouble; tempWindowBufferDouble = NULL;
  delete[] tempWindowBufferFiltered; tempWindowBufferFiltered = NULL;
  delete[] tempWindowBufferFilteredDouble; tempWindowBufferFilteredDouble = NULL;

  setFramesPerChunk(0);
  _startTime = 0.0;
  _chunkNum = 0;
  _offset = 0;
}

void SoundFile::setFilename(const QString& filename)
{
    this->filename = filename;
}

void SoundFile::setFilteredFilename(const QString& filteredFilename)
{
    this->filteredFilename = filteredFilename;
}

QString SoundFile::getNextTempFilename()
{
  QString tempFileFolder = gdata->qsettings->value("General/tempFilesFolder",
       QDir::convertSeparators(QDir::currentPath())).toString();
  QDir dir = QDir(tempFileFolder);
  QFileInfo fileInfo;
  QString fileName;
  bool fileExists;
  int index = 1;
  do {
    fileExists = false;
    fileName.sprintf("temp%03d.wav", index);
    //printf("trying %s\n", fileName.latin1());
    fileInfo.setFile(dir, fileName);
    if(fileInfo.exists()) {
      fileExists = true;
      index++;
    }
  } while(fileExists);
  return fileInfo.absoluteFilePath();
}

bool SoundFile::openRead(const char *filename_)
{
  uninit();
  setSaved(true);

  setFilename(filename_);
  setFilteredFilename(getNextTempFilename());
//#ifdef USE_SOX
//  stream = new SoxStream;
//#else
  if(filename.endsWith(".wav")) {
      stream = new WaveStream;
      filteredStream = new WaveStream;
  }
//#ifdef USE_OGG
//  else if(str_case_cmp(getFileExtension(filename), "ogg") == 0) {
//	  stream = new OggStream;
//  }
//#endif
  else {
      fprintf(stderr, "Cannot open file of this type. %s\n", S(filename));
    return false;
  }
//#endif

  if(stream->open_read(S(filename))) {
    fprintf(stderr, "Error opening %s\n", S(filename));
    return false;
  }
  if(filteredStream->open_write(S(filteredFilename), stream->freq, stream->channels, stream->bits)) {
    fprintf(stderr, "Error opening %s\n", S(filteredFilename));
    delete stream; stream = NULL;
    QString s = QString("Error opening ") + QString(filteredFilename) + QString(" for writing.\nPossible cause: temp folder is read-only or disk is out of space.\nPlease select a writable Temp Folder");
    QMessageBox::warning(gdata->mainWindow, "Error", s, QMessageBox::Ok, QMessageBox::NoButton);
    gdata->mainWindow->menuPreferences();
    return false;
  }

  channels.resize(stream->channels);
  int windowSize_ = gdata->getAnalysisBufferSize(stream->freq);
  fprintf(stderr, "channels = %d\n", numChannels());

  int stepSize_ = gdata->getAnalysisStepSize(stream->freq);
  //_offset = (windowSize_ / stepSize_) / 2;
  _offset = windowSize_ / 2;
  setFramesPerChunk(stepSize_); // The user needs to be able to set this

  for(int j=0; j<numChannels(); j++) {
    channels(j) = new Channel(this, windowSize_);
    fprintf(stderr, "channel size = %d\n", channels(j)->size());
    //channels(j)->setParent(this);
    channels(j)->setColor(gdata->getNextColor());
    //channels(j)->setPitchMethod(gdata->pitch_method[0]);
  }
  myTransforms.init(windowSize_, 0, stream->freq, gdata->doingEqualLoudness());

  fprintf(stderr, "----------Opening------------\n");
  fprintf(stderr, "filename = %s\n", filename_);
  fprintf(stderr, "rate = %d\n", rate());
  fprintf(stderr, "channels = %d\n", numChannels());
  fprintf(stderr, "bits = %d\n", bits());
  fprintf(stderr, "windowSize = %d\n", windowSize_);
  fprintf(stderr, "stepSize = %d\n", stepSize_);
  fprintf(stderr, "-----------------------------\n");

  //setup the tempChunkBuffers
  tempWindowBuffer = new float *[numChannels()];
  tempWindowBufferDouble = new double *[numChannels()];
  tempWindowBufferFiltered = new float *[numChannels()];
  tempWindowBufferFilteredDouble = new double *[numChannels()];
  for(int c=0; c<numChannels(); c++) {
    tempWindowBuffer[c] = (new float[bufferSize()+16]) + 16; //array ranges from -16 to bufferSize()
    tempWindowBufferDouble[c] = (new double[bufferSize()+16]) + 16; //array ranges from -16 to bufferSize()
    tempWindowBufferFiltered[c] = (new float[bufferSize()+16]) + 16; //array ranges from -16 to bufferSize()
    tempWindowBufferFilteredDouble[c] = (new double[bufferSize()+16]) + 16; //array ranges from -16 to bufferSize()
  }

  _doingDetailedPitch = gdata->doingDetailedPitch();

  return true;
}

bool SoundFile::openWrite(const char *filename_, int rate_, int channels_, int bits_, int windowSize_, int stepSize_)
{
  uninit();
  setSaved(false);

  //_offset = (windowSize_ / stepSize_) / 2;
  _offset = windowSize_ / 2;
  fprintf(stderr, "--------Recording------------\n");
  fprintf(stderr, "filename = %s\n", filename_);
  fprintf(stderr, "rate = %d\n", rate_);
  fprintf(stderr, "channels = %d\n", channels_);
  fprintf(stderr, "bits = %d\n", bits_);
  fprintf(stderr, "windowSize = %d\n", windowSize_);
  fprintf(stderr, "stepSize = %d\n", stepSize_);
  fprintf(stderr, "-----------------------------\n");

  setFramesPerChunk(stepSize_);

  setFilename(filename_);
  //setFilteredFilename((QString(filename)+QString("~")).ascii());
  setFilteredFilename(getNextTempFilename());
  
  stream = new WaveStream;
  filteredStream = new WaveStream;
  if(stream->open_write(S(filename), rate_, channels_, bits_)) {
    fprintf(stderr, "Error opening %s for writting\n", S(filename));
    delete stream; stream = NULL;
    delete filteredStream; filteredStream = NULL;
    QString s = QString("Error opening ") + QString(filename) + QString(" for writing.\nPossible cause: temp folder is read-only or disk is out of space.\nPlease select a writable Temp Folder");
    QMessageBox::warning(gdata->mainWindow, "Error", s, QMessageBox::Ok, QMessageBox::NoButton);
    gdata->mainWindow->menuPreferences();
    return false;
  }
  if(filteredStream->open_write(S(filteredFilename), rate_, channels_, bits_)) {
    fprintf(stderr, "Error opening %s for writting\n", S(filteredFilename));
    delete stream; stream = NULL;
    delete filteredStream; filteredStream = NULL;
    QString s = QString("Error opening ") + QString(filteredFilename) + QString(" for writing.\nPossible cause: temp folder is read-only or disk is out of space.\nPlease select a writable Temp Folder");
    QMessageBox::warning(gdata->mainWindow, "Error", s, QMessageBox::Ok, QMessageBox::NoButton);
    gdata->mainWindow->menuPreferences();
    return false;
  }

  channels.resize(stream->channels);
  fprintf(stderr, "channels = %d\n", numChannels());
  for(int j=0; j<numChannels(); j++) {
    channels(j) = new Channel(this, windowSize_);
    fprintf(stderr, "channel size = %d\n", channels(j)->size());
    //channels(j)->setParent(this);
    channels(j)->setColor(gdata->getNextColor());
    //channels(j)->setPitchMethod(gdata->pitch_method[0]);
  }
  myTransforms.init(windowSize_, 0, stream->freq, gdata->doingEqualLoudness());

  //setup the tempChunkBuffers
  tempWindowBuffer = new float*[numChannels()];
  tempWindowBufferDouble = new double*[numChannels()];
  tempWindowBufferFiltered = new float*[numChannels()];
  tempWindowBufferFilteredDouble = new double*[numChannels()];
  for(int c=0; c<numChannels(); c++) {
    tempWindowBuffer[c] = (new float[bufferSize()+16]) + 16; //array ranges from -16 to bufferSize()
    tempWindowBufferDouble[c] = (new double[bufferSize()+16]) + 16; //array ranges from -16 to bufferSize()
    tempWindowBufferFiltered[c] = (new float[bufferSize()+16]) + 16; //array ranges from -16 to bufferSize()
    tempWindowBufferFilteredDouble[c] = (new double[bufferSize()+16]) + 16; //array ranges from -16 to bufferSize()
  }

  _doingDetailedPitch = gdata->doingDetailedPitch();

  return true;
}

void SoundFile::rec2play()
{
  stream->close();
  stream->open_read(S(filename));
  filteredStream->close();
  filteredStream->open_read(S(filteredFilename));
  jumpToChunk(totalChunks());
  fprintf(stderr, "filteredFilename = %s\n", S(filteredFilename));
  fprintf(stderr, "totalChunks = %d\n", totalChunks());
}

void SoundFile::close()
{
  uninit();
}
  
/** Lock the mutex's of all the channels */
void SoundFile::lock()
{
  mutex->lock();
  for(int j=0; j<numChannels(); j++)
    channels(j)->lock();
}

/** Unlock the mutex's of all the channels */
void SoundFile::unlock()
{
  for(int j=0; j<numChannels(); j++)
    channels(j)->unlock();
  mutex->unlock();
}

/** Plays one chunk of sound to the associated stream
  @return true on success. false for end of file or error
*/
bool SoundFile::playChunk()
{
/*
  int c;
  int ret = blockingRead(stream, tempWindowBuffer, framesPerChunk());
  if(equalLoudness()) ret = blockingRead(filteredStream, tempWindowBufferFiltered, framesPerChunk());
  if(ret < framesPerChunk()) return false;
  ret = blockingWrite(gdata->audio_stream, tempWindowBuffer, framesPerChunk());
  if(ret < framesPerChunk()) {
    fprintf(stderr, "Error writing to audio device\n");
  }
  
  lock();
  for(c=0; c<numChannels(); c++) {
    channels(c)->shift_left(framesPerChunk());
    toChannelBuffer(c, framesPerChunk());
	  if(gdata->doingActive() && channels(c) == gdata->getActiveChannel()) {
      channels(c)->processChunk(currentChunk()+1);
	  }
  }
  setCurrentChunk(currentStreamChunk());
  unlock();
  return true;
*/
  if(!setupPlayChunk()) return false;
  int ret = blockingWrite(gdata->audio_stream, tempWindowBuffer, framesPerChunk());
  if(ret < framesPerChunk()) {
    fprintf(stderr, "Error writing to audio device\n");
  }
  //setCurrentChunk(currentStreamChunk());
  return true;
}

/*
bool SoundFile::recordChunk()
{
  int c;
  int ret = blockingRead(gdata->audio_stream, tempWindowBuffer, framesPerChunk());
  if(ret < framesPerChunk()) {
    fprintf(stderr, "Data lost in reading from audio device\n");
  }

  //static uint prevTime = 0;
  //uint time0 = clock();
  //if(mutex->locked()) {
	//printf("Waiting for mutex\n");
  //}

  FilterState filterState;
  
  lock();
  for(c=0; c<numChannels(); c++) {
    channels(c)->shift_left(framesPerChunk());
    std::copy(tempWindowBuffer[c], tempWindowBuffer[c]+framesPerChunk(), channels(c)->end() - framesPerChunk());
    channels(c)->highPassFilter->getState(&filterState);
    for(int j=channels(c)->size()-framesPerChunk(); j<channels(c)->size(); j++) {
      channels(c)->filteredInput.at(j) = bound(channels(c)->at(j), -1.0f, 1.0f);
    }
    //if(channels(c) == gdata->getActiveChannel())
    channels(c)->processNewChunk(&filterState);
  }
  unlock();

  //uint time1 = clock();

  ret = blockingWrite(stream, tempWindowBuffer, framesPerChunk());
  //ret = framesPerChunk();
  if(ret < framesPerChunk()) {
    fprintf(stderr, "Error writing to disk\n");
  }
  setCurrentChunk(currentStreamChunk());
  //clock_t time2 = clock();
  //printf("t=%d,%d\n", time1-time0, time2-time1);
  //prevTime = curTime;

  return true;
}
*/

void SoundFile::applyEqualLoudnessFilter(int n)
{
  int c, j;
  for(c=0; c<numChannels(); c++) {
    //printf("before: %f == %f\n", channels(c)->filterStateX1, channels(c)->highPassFilter->_x[0]);
    //printf("before: %f == %f\n", channels(c)->filterStateX2, channels(c)->highPassFilter->_x[1]);
    //printf("before: %f == %f\n", channels(c)->filterStateY1, channels(c)->highPassFilter->_y[0]);
    //printf("before: %f == %f\n", channels(c)->filterStateY2, channels(c)->highPassFilter->_y[1]);
/*
    tempWindowBufferDouble[c][-2] = channels(c)->filterStateX1;
    tempWindowBufferDouble[c][-1] = channels(c)->filterStateX2;
    tempWindowBufferFilteredDouble[c][-2] = channels(c)->filterStateY1;
    tempWindowBufferFilteredDouble[c][-1] = channels(c)->filterStateY2;
    double *b = &*(channels(c)->highPassFilter->_b.begin());
    double *a = &*(channels(c)->highPassFilter->_a.begin());
    //printf("%lf, %lf, %lf, %lf, %lf\n", b[0], b[1], b[2], a[0], a[1]);
    //channels(c)->highPassFilter->print();
    
    //convert all the tempWindowBuffer from floats to doubles in one go
    float *xFloat = tempWindowBuffer[c];
    double *x = tempWindowBufferDouble[c];
    float *xFloatEnd = xFloat + n;
    while(xFloat != xFloatEnd)
      *x++ = double(*xFloat++);
    
    x = tempWindowBufferDouble[c];
    float *yFloat = tempWindowBufferFiltered[c];
    double *y = tempWindowBufferFilteredDouble[c];
    
    for(j=0; j<n; j++) {
      *y = b[0]*x[0] + b[1]*x[-1] + b[2]*x[-2] - a[0]*y[-1] - a[1]*y[-2];
      // *y = y[-1];
      // *y = b[0]*x[-2] + b[1]*x[-1] + b[2]*x[0] - a[0]*y[-2] - a[1]*y[-1];
      // *y = (y[-2] + y[-1] + x[0]) / 3.0f;
      *yFloat = bound(*y, -1.0, 1.0);
      y++;
      x++;
      yFloat++;
      //tempWindowBuffer2[c][j] = bound(double(tempWindowBuffer[c][j]), -1.0, 1.0);
      //tempWindowBuffer2[c][j] = bound(filter->apply(tempWindowBuffer[c][j]), -1.0, 1.0);
    }
    channels(c)->filterStateX1 = tempWindowBufferDouble[c][n-2];
    channels(c)->filterStateX2 = tempWindowBufferDouble[c][n-1];
    channels(c)->filterStateY1 = tempWindowBufferFilteredDouble[c][n-2];
    channels(c)->filterStateY2 = tempWindowBufferFilteredDouble[c][n-1];
*/

    //float *tempTest = (float*)malloc(n*sizeof(float));
    //channels(c)->highPassFilter->apply(tempWindowBuffer[c], tempTest, n);
    channels(c)->highPassFilter->filter(tempWindowBuffer[c], tempWindowBufferFiltered[c], n);
    for(j=0; j<n; j++) tempWindowBufferFiltered[c][j] = bound(tempWindowBufferFiltered[c][j], -1.0f, 1.0f);
    //for(j=0; j<n; j++)
    //  if(tempWindowBufferFiltered[c][j] != tempTest[j]) printf("Filter Diff %d : %f, %f, %f\n", j, tempWindowBuffer[c][j], tempWindowBufferFiltered[c][j], tempTest[j]);
    //printf("after: %f == %f\n", channels(c)->filterStateX1, channels(c)->highPassFilter->_x[0]);
    //printf("after: %f == %f\n", channels(c)->filterStateX2, channels(c)->highPassFilter->_x[1]);
    //printf("after: %f == %f\n", channels(c)->filterStateY1, channels(c)->highPassFilter->_y[0]);
    //printf("after: %f == %f\n", channels(c)->filterStateY2, channels(c)->highPassFilter->_y[1]);
    //free(tempTest);
  }
}

//void SoundFile::initRecordingChunk()
void SoundFile::recordChunk(int n)
{
  //int n = offset();
  int c;
  int ret = blockingRead(gdata->audio_stream, tempWindowBuffer, n);
  if(ret < n) {
    fprintf(stderr, "Data lost in reading from audio device\n");
  }
  
/*  if(equalLoudness()) applyEqualLoudnessFilter(n);
  
  FilterState filterState;
  
  lock(); 
  for(c=0; c<numChannels(); c++) {
    channels(c)->shift_left(n);
    //std::copy(tempWindowBuffer[c], tempWindowBuffer[c]+n, channels(c)->end() - n);
    //channels(c)->highPassFilter->getState(&filterState);
    toChannelBuffer(c, n);
    //if(channels(c) == gdata->getActiveChannel())
    channels(c)->processNewChunk(&filterState);
  }
  unlock();
  
  ret = blockingWrite(stream, tempWindowBuffer, n);
  if(ret < n) fprintf(stderr, "Error writing to disk\n");
  if(equalLoudness()) ret = blockingWrite(filteredStream, tempWindowBufferFiltered, n);
  if(ret < n) fprintf(stderr, "Error writing to disk\n");
  
  setCurrentChunk(currentStreamChunk());
*/
  finishRecordChunk(n);
}

void SoundFile::finishRecordChunk(int n)
{
  if(equalLoudness()) applyEqualLoudnessFilter(n);

  lock(); 
  for(int c=0; c<numChannels(); c++) {
    channels(c)->shift_left(n);
    //std::copy(tempWindowBuffer[c], tempWindowBuffer[c]+n, channels(c)->end() - n);
    //channels(c)->highPassFilter->getState(&filterState);
    toChannelBuffer(c, n);
    //if(channels(c) == gdata->getActiveChannel())
    channels(c)->processNewChunk();
  }
  unlock();

  int ret = blockingWrite(stream, tempWindowBuffer, n);
  if(ret < n) fprintf(stderr, "Error writing to disk\n");
  if(equalLoudness()) ret = blockingWrite(filteredStream, tempWindowBufferFiltered, n);
  if(ret < n) fprintf(stderr, "Error writing to disk\n");

  setCurrentChunk(currentStreamChunk());
}

bool SoundFile::setupPlayChunk() {
  int c;
  int n = framesPerChunk();
  int ret = blockingRead(stream, tempWindowBuffer, n);
  if(equalLoudness()) ret = blockingRead(filteredStream, tempWindowBufferFiltered, n);
  if(ret < n) return false;

  lock();
  for(c=0; c<numChannels(); c++) {
    channels(c)->shift_left(n);
    toChannelBuffer(c, n);
    if(gdata->doingActive() && channels(c) == gdata->getActiveChannel()) {
      channels(c)->processChunk(currentChunk()+1);
    }
  }
  setCurrentChunk(currentStreamChunk());
  unlock();
  return true;
}

/** Plays one chunk of sound to the associated stream
  @return true on success. false for end of file or error
*/
//Note: static
bool SoundFile::playRecordChunk(SoundFile *play, SoundFile *rec)
{
  int n = rec->framesPerChunk();
  myassert(n == play->framesPerChunk());
  //int ch = rec->numChannels();
  myassert(rec->numChannels() == play->numChannels());

  play->setupPlayChunk();
  //if(!play->setupPlayChunk()) return false;

  int ret = blockingWriteRead(gdata->audio_stream, play->tempWindowBuffer, play->numChannels(), rec->tempWindowBuffer, rec->numChannels(), n);
  if(ret < n) {
    fprintf(stderr, "Error writing/reading to audio device\n");
  }

  rec->finishRecordChunk(n);
  return true;
}

/** Reads framesPerChunk frames from the SoundStream s
    into the end of the channel buffers. I.e losing framesPerChunk
    frames from the beginning of the channel buffers.
    If s is NULL (the defult) the the file stream is used
  @param s The SoundStream to read from or NULL (the default) the current file stream is used    
  @return The number of frames actually read ( <= framesPerChunk() )
*/
//int SoundFile::readChunk(int n, SoundStream *s)
int SoundFile::readChunk(int n)
{
  int c;
  //if(!s) s = stream;
  
  int ret = blockingRead(stream, tempWindowBuffer, n);
  if(equalLoudness()) {
    applyEqualLoudnessFilter(n);
    ret = blockingWrite(filteredStream, tempWindowBufferFiltered, ret);
  }
  if(ret < n) return ret;
  
  lock();

  //nextChunk();
  for(c=0; c<numChannels(); c++) {
    channels(c)->shift_left(n);
    //channels(c)->highPassFilter->getState(&filterState);
    toChannelBuffer(c, n);
    channels(c)->processNewChunk();
  }
  //incrementChunkNum();
  setCurrentChunk(currentStreamChunk());
  unlock();
  return ret;
}

/** Waits until there is n frames of data to read from s,
  Then puts the data in buffer.
  @param s The SoundStream to read from
  @param buffer The chunk buffer to put the data into. (s->channels of at least n length)
    This is padded with zeros (up to n) if there is less than n frames read.
  @param n The number of frames to read
  @return The number of frames read.
*/
int SoundFile::blockingRead(SoundStream *s, float **buffer, int n)
{ 
  int c;
  myassert(s);
  myassert(n >= 0 && n <= bufferSize());
  int ret = s->readFloats(buffer, n, numChannels());
  if(ret < n) { //if not all bytes are read
    for(c=0; c<numChannels(); c++) { //set remaining bytes to zeros
      //memset(buffer[c]+ret, 0, (n-ret) * sizeof(float)); 
      std::fill(buffer[c]+ret, buffer[c]+n, 0.0f);
    }
  }
  return ret;
}

/** Writes n frames of data to s from buffer.
  @param s The SoundStream to write to
  @param buffer The chunk buffer data to use. (s->channels of at least n length)
  @param n The number of frames to write
  @return The number of frames written.
*/
int SoundFile::blockingWrite(SoundStream *s, float **buffer, int n)
{
  if(s == NULL) return n;
  return s->writeFloats(buffer, n, numChannels());
}

/** Writes n frames of data to s and reads n frames of data from s
If less than n frams are read, the remaining buffer is filled with zeros
@param s The SoundStream to write to then read from
@param writeBuffer The data that is to be written to the stream
@param readBuffer The data that is read back from the stream
@param n The amount of data in each of the buffers. Note: They must be the same size
@param ch The number of channels
*/
//Note: static
int SoundFile::blockingWriteRead(SoundStream *s, float **writeBuffer, int writeCh, float **readBuffer, int readCh, int n)
{
  int c;
  myassert(s);
  //myassert(n >= 0 && n <= bufferSize());
  int ret = s->writeReadFloats(writeBuffer, writeCh, readBuffer, readCh, n);
  if(ret < n) { //if not all bytes are read
    for(c=0; c<readCh; c++) { //set remaining bytes to zeros
      //memset(buffer[c]+ret, 0, (n-ret) * sizeof(float)); 
      std::fill(readBuffer[c]+ret, readBuffer[c]+n, 0.0f);
    }
  }
  return ret;
}

/**
  @param c Channel number
  @param n Number of frames to copy from tempWindowBuffer into channel
*/
void SoundFile::toChannelBuffer(int c, int n)
{
  std::copy(tempWindowBuffer[c], tempWindowBuffer[c]+n, channels(c)->end() - n);
  if(equalLoudness()) {
    std::copy(tempWindowBufferFiltered[c], tempWindowBufferFiltered[c]+n, channels(c)->filteredInput.end() - n);
  
/*
    for(int j=channels(c)->size()-n; j<channels(c)->size(); j++) {
      channels(c)->filteredInput.at(j) = bound(channels(c)->highPassFilter->apply(channels(c)->at(j)), -1.0, 1.0);
    }
    //channels(c)->filteredData.push_back(channels(c)->filteredInput.end()-n, n);
*/
  }
}

void SoundFile::toChannelBuffers(int n)
{
  int c;
  lock();
  myassert(n >= 0 && n <= bufferSize());
  for(c=0; c<numChannels(); c++) {
    channels(c)->shift_left(n);
    toChannelBuffer(c, n);
  }
  unlock();
}

//int SoundFile::readN(SoundStream *s,int n)
int SoundFile::readN(int n)
{
  myassert(n >= 0 && n <= bufferSize());
  int ret = blockingRead(stream, tempWindowBuffer, n);
  if(equalLoudness()) ret = blockingRead(filteredStream, tempWindowBufferFiltered, n);
  toChannelBuffers(n);
  return ret;
}

/** Writes framesPerChunk frames into the SoundStream s
    from end of the channel buffers.
    If s is NULL (the defult) then the file stream is used
  @param s The SoundStream to write to, or NULL (the default) the current file stream is used    
  @return The number of frames actually written ( <= framesPerChunk() )
*/
/*
int SoundFile::writeChunk(SoundStream *s)
{
  if(!s) s = stream;
  //myassert(numChannels() == s->channels);
  lock();
  for(int c=0; c<numChannels(); c++)
    std::copy(channels(c)->end() - framesPerChunk(), channels(c)->end(), tempWindowBuffer[c]);
  unlock();
  int ret = s->writeFloats(tempWindowBuffer, framesPerChunk(), numChannels());
  return ret;
}
*/

/** Process the chunks for all the channels
   Increment the chunkNum.
*/

void SoundFile::processNewChunk()
{
  for(int j=0; j<numChannels(); j++) {
    channels(j)->lock();
    //channels(j)->highPassFilter->getState(&filterState);
    channels(j)->processNewChunk();
    channels(j)->unlock();
  }
}

/** Preprocess the whole sound file,
   by looping through and processing every chunk in the file.
   A progress bar is displayed in the toolbar, because this can
   be time consuming.
*/
void SoundFile::preProcess()
{
  gdata->setDoingActiveAnalysis(true);
  myassert(firstTimeThrough == true);
  readChunk(bufferSize() - offset());

  // Create a progress bar in the status bar to tell the user we're preprocessing
  QStatusBar *theStatusBar = gdata->mainWindow->statusBar();
  QLabel *message = new QLabel("Preprocessing data:", theStatusBar);

  QProgressBar *progress = new QProgressBar(theStatusBar);
  progress->setMaximum(stream->totalFrames() / framesPerChunk());
  progress->setValue(0);
  progress->setMaximumHeight(16);

  theStatusBar->addWidget(message);
  theStatusBar->addWidget(progress);

  message->show();
  progress->show();

  int frameCount = 1;
  int updateInterval = MAX(1, progress->maximum() / 50); // We'll update 50 times only

  while(readChunk(framesPerChunk()) == framesPerChunk()) { // put data in channels
    frameCount++;

    if (frameCount % updateInterval == 0) {
         progress->setValue(progress->value() + updateInterval);
         qApp->processEvents();
         frameCount = 1;
    }
  }
  filteredStream->close();
  filteredStream->open_read(S(filteredFilename));
  jumpToChunk(0);


  progress->setValue(progress->maximum());
  theStatusBar->removeWidget(progress);
  theStatusBar->removeWidget(message);
  delete progress;
  delete message;

  gdata->setDoingActiveAnalysis(false);
  firstTimeThrough = false;
}

//shift all the channels data left by n frames
void SoundFile::shift_left(int n)
{
  for(int j=0; j<numChannels(); j++) {
    channels(j)->shift_left(n);
  }
}

void SoundFile::jumpToChunk(int chunk)
{
  int c;
  lock();
  int pos = chunk * framesPerChunk() - offset();
  if(pos < 0) {
    stream->jump_to_frame(0);
    if(equalLoudness()) filteredStream->jump_to_frame(0);
    for(c=0; c<numChannels(); c++) {
      channels(c)->reset();
    }
    readN(bufferSize() + pos);
  } else {
    stream->jump_to_frame(pos);
    if(equalLoudness()) filteredStream->jump_to_frame(pos);
    readN(bufferSize());
  }
  setCurrentChunk(chunk);
  unlock();
}

int SoundFile::bufferSize() {
  myassert(!channels.isEmpty());
  return channels(0)->size();
}

int SoundFile::totalChunks() {
  myassert(!channels.isEmpty());
  return channels(0)->totalChunks();
}

bool SoundFile::inFile()
{
  int c = currentChunk();
  return (c >= 0 && c < totalChunks());
}
