/***************************************************************************
                          channel.cpp  -  description
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

#include "channel.h"
#include "gdata.h"
#include "fast_smooth.h"
#include "analysisdata.h"
#include "mystring.h"
#include "notedata.h"
#include "musicnotes.h"
#include "mainwindow.h"
#include "IIR_Filter.h"
#include <QtGui/QFileDialog>
#include <QtCore/QSettings>
#include <QtCore/QTextStream>

/*
This filter was created in Matlab using
[Bb,Ab]=butter(2,(150/(sampleRate/2)),'high');
num2str(Bb, '%1.52f\n')
num2str(Ab, '%1.52f\n')
*/

double highPassFilterCoeffB[6][3] = {
  { //96000 Hz
    0.9930820351754101604768720790161751210689544677734375,
    -1.9861640703508203209537441580323502421379089355468750,
    0.9930820351754101604768720790161751210689544677734375
  },  { //48000 Hz
    0.9862119246270822925382049106701742857694625854492188,
    -1.9724238492541645850764098213403485715389251708984375,
    0.9862119246270822925382049106701742857694625854492188
  },  { //44100 Hz
    0.9850017578724193922923291211191099137067794799804688,
    -1.9700035157448387845846582422382198274135589599609375,
    0.9850017578724193922923291211191099137067794799804688
  },  { //22050 Hz
    0.9702283766983297308428291216841898858547210693359375,
    -1.9404567533966594616856582433683797717094421386718750,
    0.9702283766983297308428291216841898858547210693359375
  },  { //11025 Hz
    0.9413417959923573441471944533986970782279968261718750,
    -1.8826835919847146882943889067973941564559936523437500,
    0.9413417959923573441471944533986970782279968261718750
  },  { //8000 Hz
    0.9200661584291680572533778104116208851337432861328125,
    -1.8401323168583361145067556208232417702674865722656250,
    0.9200661584291680572533778104116208851337432861328125
  }
};

double highPassFilterCoeffA[6][3] = {
  { //96000 Hz
    1.0,
    -1.9861162115408896866597387997899204492568969726562500,
    0.9862119291607510662700519787904340773820877075195312
  },  { //48000 Hz
    1.0,
    -1.9722337291952660720539824978914111852645874023437500,
    0.9726139693130629870765346822736319154500961303710938
  },  { //44100 Hz
    1.0,
    -1.9697785558261799998547303403029218316078186035156250,
    0.9702284756634975693145861441735178232192993164062500
  },  { //22050 Hz
    1.0,
    -1.9395702073516702945710221683839336037635803222656250,
    0.9413432994416484067556893933215178549289703369140625
  },  { //11025 Hz
    1.0,
    -1.8792398422342264652229459898080676794052124023437500,
    0.8861273417352029113658318237867206335067749023437500
  },  { //8000 Hz
    1.0,
    -1.8337326589246480956774121295893564820289611816406250,
    0.8465319747920239112914941870258189737796783447265625
  }
};

/*
This filter was created in matlab using
0  [Bb,Ab]=butter(2,1000/(44100/2),'low');
   num2str(Bb, '%1.52f\n')
   num2str(Ab, '%1.52f\n')

1  [Bb,Ab]=butter(2,0.25,'low');

2  [Bb,Ab]=butter(2,50/(44100/2),'low');
*/
double lowPassFilterCoeffB[3][3] = {
  { //44100 Hz
    0.0046039984750224638432314350211527198553085327148438,
    0.0092079969500449276864628700423054397106170654296875,
    0.0046039984750224638432314350211527198553085327148438
  }, {
    0.0976310729378175312653809214680222794413566589355469,
    0.1952621458756350625307618429360445588827133178710938,
    0.0976310729378175312653809214680222794413566589355469
  }, {
    0.0000126234651238732453748525585979223251342773437500,
    0.0000252469302477464907497051171958446502685546875000,
    0.0000126234651238732453748525585979223251342773437500
  }
};

double lowPassFilterCoeffA[3][3] = {
  { //44100 Hz
    1.0,
    -1.7990964094846682019834815946524031460285186767578125,
    0.8175124033847580573564073347370140254497528076171875
  }, {
    1.0,
    -0.9428090415820633563015462641487829387187957763671875,
    0.3333333333333333703407674875052180141210556030273438
  }, {
    1.0,
    -1.9899255200849252922523646702757105231285095214843750,
    0.9899760139454206742115616179944481700658798217773438
  }
};

Channel::Channel(SoundFile *parent, int size, int k)
    :ChannelBase(parent->timePerChunk(), parent->rate(),
          parent->framesPerChunk(), size, k)
{
  setParent(parent);
  _pitch_method = 2;
  color = Qt::black;
  rmsFloor = gdata->dBFloor();
  rmsCeiling = gdata->dBFloor();

  visible = true;
  setIntThreshold(gdata->qsettings->value("Analysis/thresholdValue", 93).toInt());
  freq = 0.0;
  mutex = new QMutex(QMutex::Recursive);
  isLocked = false;
  int filterIndex = 2;
  //choose the closest filter index
  if(sampleRate > (48000 + 96000) / 2) filterIndex = 0; //96000 Hz
  else if(sampleRate > (44100 + 48000) / 2) filterIndex = 1; //48000 Hz
  else if(sampleRate > (22050 + 44100) / 2) filterIndex = 2; //44100 Hz
  else if(sampleRate > (11025 + 22050) / 2) filterIndex = 3; //22050 Hz
  else if(sampleRate > (8000 + 11025) / 2) filterIndex = 4; //11025 Hz
  else filterIndex = 5; //8000 Hz
  highPassFilter = new IIR_Filter(highPassFilterCoeffB[filterIndex], highPassFilterCoeffA[filterIndex], 3);
}

Channel::~Channel()
{
  delete mutex;
  delete highPassFilter;
}

void Channel::resetIntThreshold(int thresholdPercentage)
{
  _threshold = float(thresholdPercentage) / 100.0f;
  uint j;
  for(j=0; j<lookup.size(); j++) {
    chooseCorrelationIndex(j, periodOctaveEstimate(j), gdata->analysisType(),
                           gdata->topPitch());
    calcDeviation(j);
  }
  clearFreqLookup();
}

void Channel::shift_left(int n)
{
  directInput.shift_left(n);
  filteredInput.shift_left(n);
}

/** Analysis the current data and add it to the end of the lookup table
  Note: The Channel shoud be locked before calling this.
*/
void Channel::processNewChunk()
{
  myassert(locked());
  myassert(parent->currentRawChunk() == MAX(0, parent->currentStreamChunk()-1) ||
           parent->currentRawChunk() == MAX(0, parent->currentStreamChunk()));
  AnalysisData data;
  data.dbfloor = dBFloor();
  lookup.push_back(data);
  parent->myTransforms.calculateAnalysisData(int(lookup.size())-1, this,
                                             gdata->analysisType(),
                                             gdata->ampThresholds,
                                             gdata->doingFreqAnalysis(),
                                             gdata->doingActiveAnalysis(),
                                             gdata->doingAutoNoiseFloor(),
                                             gdata->doingHarmonicAnalysis(),
                                             gdata->rmsFloor(),
                                             gdata->rmsCeiling(),
                                             gdata->topPitch(),
                                             gdata->ampThreshold(NOTE_SCORE,0));
}

/** Analysis the current data and put it at chunk position in the lookup table
  Note: The Channel shoud be locked before calling this.
  @param chunk The chunk number to store the data at
*/
void Channel::processChunk(int chunk)
{
  myassert(locked());
  if(chunk >= 0 && chunk < totalChunks()) {
    parent->myTransforms.calculateAnalysisData(/*begin(), */chunk, this/*, threshold()*/,
                                               gdata->analysisType(),
                                               gdata->ampThresholds,
                                               gdata->doingFreqAnalysis(),
                                               gdata->doingActiveAnalysis(),
                                               gdata->doingAutoNoiseFloor(),
                                               gdata->doingHarmonicAnalysis(),
                                               gdata->rmsFloor(),
                                               gdata->rmsCeiling(),
                                               gdata->topPitch(),
                                               gdata->ampThreshold(NOTE_SCORE,0));
  }
}

void Channel::reset()
{
  std::fill(begin(), end(), 0.0f);
  std::fill(filteredInput.begin(), filteredInput.end(), 0.0f);
}

//returns the filename part of a full path
QString getFilenamePart(const QString& filename)
{
    return QFileInfo(filename).fileName();
}

QString Channel::getUniqueFilename()
{


    QString endingStar = (parent->saved()) ? QString("") : QString("*");

    if (getParent()->channels.size() == 1) {
        return QString(getFilenamePart(getParent()->filename)) + endingStar;
    } else {
        for (int i = 0; i < getParent()->channels.size(); i++) {
            if ( getParent()->channels.at(i) == this ) {
                return QString(getFilenamePart(getParent()->filename)) + " (" + QString::number(i+1) + ")" + endingStar;
            }
        }
    }

    // If we're here, we didn't find the channel in the parent's channels array.
    // This should never happen!
    myassert(false);
    return QString(getFilenamePart(getParent()->filename)) + endingStar;
}

/**
 * Returns the average pitch for a given channel between two frames.
 * It weights using a -ve cos shaped window
 *
 * @param begin the starting frame number.
 * @param end   the ending frame number.
 *
 * @return the average pitch, or -1 if there were no valid pitches.
 **/
float Channel::averagePitch(int begin, int end)
{
  if(begin < 0) begin = 0;
  if (begin >= end) return -1;
  myassert(locked());
  myassert(isValidChunk(begin) && isValidChunk(end-1));

  // Init the total to be the first item if certain enough or zero if not

  //do a weighted sum (using cosine window smoothing) to find the average note
  double goodCount = 0.0;
  double total = 0.0;
  double size = double(end - begin);
  double window_pos, window_weight, weight;
  AnalysisData *data;
  for (int i = begin; i < end; i++) {
    window_pos = double(i - begin) / size;
    window_weight = 0.5 - 0.5 * cos(window_pos * (2 * PI));
    data = dataAtChunk(i);
    weight = window_weight * data->correlation() * dB2Linear(data->logrms());
    total += data->pitch * weight;
    goodCount += weight;
  }
  return float(total / goodCount);
}

void Channel::clearFreqLookup()
{
  ChannelLocker channelLocker(this);
  summaryZoomLookup.clear();
  normalZoomLookup.clear();
}

void Channel::clearAmplitudeLookup()
{
  ChannelLocker channelLocker(this);
  amplitudeZoomLookup.clear();
}

void Channel::recalcScoreThresholds()
{
  AnalysisData *d;
  ChannelLocker channelLocker(this);
  for(int j=0; j<totalChunks(); j++) {
    if((d = dataAtChunk(j)) != NULL) {
        d->calcScores(gdata->ampThresholds);
    }
  }
}

/**
  @param noteIndex_ the index of the note to inquire about
  @return true if the loudest part of the note is above the noiseThreshold
*/
bool Channel::isVisibleNote(int noteIndex_)
{
  myassert(noteIndex_ < (int)noteData.size());
  if(noteIndex_ == NO_NOTE) return false;
  return true;
}

/**
  @param noteIndex_ the index of the note to inquire about
  @return true if the note is long enough
*/
bool Channel::isLabelNote(int noteIndex_)
{
  myassert(noteIndex_ < (int)noteData.size());
  if(noteIndex_ >= 0 && noteData[noteIndex_].isValid()) return true;
  else return false;
}

NoteData *Channel::getCurrentNote()
{
  AnalysisData *analysisData = dataAtCurrentChunk();
  if(analysisData) {
    int noteIndex = analysisData->noteIndex;
    if(noteIndex >= 0 && noteIndex < (int)noteData.size()) return &noteData[noteIndex];
  }
  return NULL;
}

void Channel::exportChannel(int type, QString typeString)
{
  QString s = QFileDialog::getSaveFileName(gdata->mainWindow, "Choose a filename to save under", ".", typeString);
  //printf("file = %s\n", s.latin1());
  if(s.isEmpty()) return;

  QFile f(s);
  f.open(QIODevice::WriteOnly);
  QTextStream out(&f);
  if(type == 0) { //plain text
    out << "        Time(secs) Pitch(semi-tones)       Volume(rms)" << endl;
    out << qSetFieldWidth(18);
    for(int j=0; j<totalChunks(); j++) {
      out << timeAtChunk(j) <<  dataAtChunk(j)->pitch << dataAtChunk(j)->logrms() << endl;
    }
  } else if(type == 1) { //matlab file
    out << "t = [";
    for(int j=0; j<totalChunks(); j++) {
      if(j>0) out << ", ";
      out << timeAtChunk(j);
    }
    out << "];" << endl;

    out << "pitch = [";
    for(int j=0; j<totalChunks(); j++) {
      if(j>0) out << ", ";
      out << dataAtChunk(j)->pitch;
    }
    out << "];" << endl;

    out << "volume = [";
    for(int j=0; j<totalChunks(); j++) {
      if(j>0) out << ", ";
      out << dataAtChunk(j)->logrms();
    }
    out << "];" << endl;
  }
  f.close();
}

double Channel::dBFloor() {
    return gdata->dBFloor();
}

