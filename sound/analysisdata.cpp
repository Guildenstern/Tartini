/***************************************************************************
                          analysisdata.cpp  -  description
                             -------------------
    begin                : Fri Dec 17 2004
    copyright            : (C) 2004 by Philip McLeod
    email                : pmcleod@cs.otago.ac.nz
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
 ***************************************************************************/

#include "analysisdata.h"
#include "useful.h"

AnalysisData::AnalysisData()
{
  dbfloor = -150.0f;
  std::fill(values, values+NUM_AMP_MODES, 0.0f);
  period = 0.0f;
  fundamentalFreq = 0.0f;
  pitch = 0.0f;
  _freqCentroid = 0.0f;
  pitchSum = 0.0f;
  pitch2Sum = 0.0;
  shortTermMean = 0.0;
  shortTermDeviation = 0.0;
  longTermMean = 0.0;
  longTermDeviation = 0.0;
  spread = spread2 = 0.0;
  vibratoPitch = 0.0f;
  vibratoWidth = 0.0f;
  vibratoSpeed = 0.0f;
  vibratoWidthAdjust = 0.0f;
  vibratoPhase = 0.0f;
  vibratoError = 0.0f;
  reason = 0;
  //volumeValue = 0.0f;
  highestCorrelationIndex = -1;
  chosenCorrelationIndex = -1;
  periodRatio = 1.0f;
  cepstrumIndex = -1;
  cepstrumPitch = 0.0f;
  //periodOctaveEstimate = -1.0f;
  noteIndex = NO_NOTE;
  notePlaying = false;
  done = false;
/*
  std::vector<float> periodEstimates;
  std::vector<float> periodEstimatesAmp;
  std::vector<float> harmonicAmpNoCutOff;
  std::vector<float> harmonicAmpRelative;
  std::vector<float> harmonicAmp;
  std::vector<float> harmonicFreq;
  std::vector<float> harmonicNoise;
  FilterState filterState; //the state of the filter at the beginning of the chunk
*/
}

double AnalysisData::dBFloor() {
    return dbfloor;
}

/*
bool AnalysisData::isValid()
{
  if(rms > gdata->noiseThreshold()) return true;
  return false;
}
*/
void AnalysisData::calcScores(double ampThresholds[NUM_AMP_MODES][2])
{
  double a[NUM_AMP_MODES-2];
  int j;
  //double temp = 0.0;
  for(j=0; j<NUM_AMP_MODES-2; j++) {
    a[j] = bound(((*amp_mode_func[j])(values[j], dBFloor())
                  - (*amp_mode_func[j])(ampThresholds[j][0], dBFloor()))
                 / ((*amp_mode_func[j])(ampThresholds[j][1], dBFloor())
                    - (*amp_mode_func[j])(ampThresholds[j][0], dBFloor())),
                 0.0, 1.0);
  }
/*
  bool aYes = false;
  bool aNo = false;
  for(j=0; j<NUM_AMP_MODES-1; j++) {
    if(a[j] == 0.0) aNo = true;
    if(a[j] == 1.0) aYes = true;
    temp += a[j] * gdata->ampWeight(j);
  }
  if(aNo == true) noteScore() = 0.0;
  else if(aYes == true) noteScore() = 1.0;
  else noteScore() = temp;
*/
  noteScore() = a[AMPLITUDE_RMS] * a[AMPLITUDE_CORRELATION];
  //noteChangeScore() = a[AMPLITUDE_CORRELATION] * a[FREQ_CHANGENESS] * a[DELTA_FREQ_CENTROID];
  //noteChangeScore() = ((1.0 - a[AMPLITUDE_CORRELATION]) + (1.0 - a[FREQ_CHANGENESS]) + (a[DELTA_FREQ_CENTROID])) / 3.0;
  noteChangeScore() = (1.0 - a[FREQ_CHANGENESS]);
}
