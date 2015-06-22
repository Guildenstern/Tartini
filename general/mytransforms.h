/***************************************************************************
                          mytransforms.h  -  description
                             -------------------
    begin                : Sat Dec 11 2004
    copyright            : (C) 2004 by Philip McLeod
    email                : pmcleod@cs.otago.ac.nz
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   Please read LICENSE.txt for details.
 ***************************************************************************/

#ifndef MYTRANSFORMS_H
#define MYTRANSFORMS_H

#include <vector>
#include "modes.h"

class fast_smooth;
class ChannelBase;
class AnalysisData;

class MyTransforms
{
public:
    MyTransforms();
    ~MyTransforms();
    void init(int n_, int k_, double rate_, bool equalLoudness_=false, int numHarmonics_=40);
    static int findNSDFMaxima(float *input, int len, std::vector<int> &maxPositions);
    void calculateAnalysisData(int chunk, ChannelBase *ch, int analysisType,
                               double ampThresholds[NUM_AMP_MODES][2],
                               bool doingFreqAnalysis,
                               bool doingActiveAnalysis,
                               bool doingAutoNoiseFloor,
                               bool doingHarmonicAnalysis,
                               double& rmsFloor,
                               double& rmsCeiling,
                               double topPitch,
                               double noteThreshold);
    bool equalLoudness() const;
private:
  class Private;
  Private * const d;
};

#endif
