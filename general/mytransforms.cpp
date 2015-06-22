/***************************************************************************
                          mytransforms.cpp  -  description
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

#include "mytransforms.h"
#include "useful.h"
#include "fast_smooth.h"
#include "myassert.h"
#include "channelbase.h"
#include "analysisdata.h"
#include "bspline.h"
#include "musicnotes.h"
#include "notedata.h"
#include <fftw3.h>

class MyTransforms::Private {
public:
    int n, k, size; //n = size of data, k = size of padding for autocorrelation, size = n+k
    fftwf_plan planDataTime2FFT, planDataFFT2Time;
    fftwf_plan planAutocorrTime2FFT, planAutocorrFFT2Time;
    float *dataTemp;
    float *dataTime, *dataFFT;
    float *autocorrTime, *autocorrFFT;
    float *hanningCoeff, hanningScalar;
    float *harmonicsAmpLeft, *harmonicsPhaseLeft;
    float *harmonicsAmpCenter, *harmonicsPhaseCenter;
    float *harmonicsAmpRight, *harmonicsPhaseRight;
    bool beenInit;
    double freqPerBin;
    double rate;
    int numHarmonics;
    fast_smooth *fastSmooth;
    bool equalLoudness;

    Private();
    ~Private();
    void init(int n_, int k_, double rate_, bool equalLoudness_, int numHarmonics_);
    void uninit();  double autocorr(float *input, float *output);
    void calculateAnalysisData(int chunk, ChannelBase *ch,
                                             int analysisType,
                                             double ampThresholds[NUM_AMP_MODES][2],
                                             bool doingFreqAnalysis,
                                             bool doingActiveAnalysis,
                                             bool doingAutoNoiseFloor,
                                             bool doingHarmonicAnalysis,
                                             double& rmsFloor,
                                             double& rmsCeiling,
                                             double topPitch,
                                             double noteThreshold);
    double autoLogCorr(float *input, float *output);
    double asdf(float *input, float *output);
    double nsdf(float *input, float *output, int analysisType);
    static int findNSDFsubMaximum(float *input, int len, float threshold);
    void doChannelDataFFT(ChannelBase *ch, float *curInput, int chunk, int analysisType);
    void calcHarmonicAmpPhase(float *harmonicAmp, float *harmonicPhase, int binsPerHarmonic);
    void doHarmonicAnalysis(float *input, AnalysisData &analysisData, double period, double ampThreshold);
    float get_fine_clarity_measure(double period);
    double get_max_note_change(float *input, double period);
    void applyHanningWindow(float *d);
    static double calcFreqCentroid(float *buffer, int len);
    static double calcFreqCentroidFromLogMagnitudes(float *buffer, int len);
};

MyTransforms::Private::Private() {
    beenInit = false;
}

MyTransforms::Private::~Private() {
    uninit();
}

MyTransforms::MyTransforms() :d(new Private())
{
}

MyTransforms::~MyTransforms()
{
    delete d;
}

bool MyTransforms::equalLoudness() const {
    return d->equalLoudness;
}

/** init() Initalises the parameters of a class instance. This must be called before use
  @param n The size of the data windows to be processed
  @param k The number of outputs wanted (autocorr size = n_ + k_). Set k_ = 0, to get default n_/2
  @param rate The sampling rate of the incoming signal to process
*/
void MyTransforms::init(int n, int k, double rate, bool equalLoudness, int numHarmonics)
{
    d->init(n, k, rate, equalLoudness, numHarmonics);
}

void MyTransforms::Private::init(int n_, int k_, double rate_, bool equalLoudness_, int numHarmonics_)
{
  const int myFFTMode = FFTW_ESTIMATE;
  //const int myFFTMode = FFTW_MEASURE;
  //const int myFFTMode = FFTW_PATIENT;
  uninit();
  if(k_ == 0) k_ = (n_ + 1) / 2;
  
  n = n_;
  k = k_;
  size = n + k;
  rate = rate_;
  equalLoudness = equalLoudness_;
  numHarmonics = numHarmonics_;

  dataTemp = (float*)fftwf_malloc(sizeof(float) * n);
  dataTime = (float*)fftwf_malloc(sizeof(float) * n);
  dataFFT  =  (float*)fftwf_malloc(sizeof(float) * n);
  autocorrTime = (float*)fftwf_malloc(sizeof(float) * size);
  autocorrFFT  = (float*)fftwf_malloc(sizeof(float) * size);
  hanningCoeff  = (float*)fftwf_malloc(sizeof(float) * n);

  planAutocorrTime2FFT = fftwf_plan_r2r_1d(size, autocorrTime, autocorrFFT, FFTW_R2HC, myFFTMode);
  planAutocorrFFT2Time = fftwf_plan_r2r_1d(size, autocorrFFT, autocorrTime, FFTW_HC2R, myFFTMode);
  
  planDataTime2FFT = fftwf_plan_r2r_1d(n, dataTime, dataFFT, FFTW_R2HC, myFFTMode);
  planDataFFT2Time = fftwf_plan_r2r_1d(n, dataFFT, dataTime, FFTW_HC2R, myFFTMode); //???

  harmonicsAmpLeft = (float*)fftwf_malloc(numHarmonics * sizeof(float));
  harmonicsPhaseLeft = (float*)fftwf_malloc(numHarmonics * sizeof(float));
  harmonicsAmpCenter = (float*)fftwf_malloc(numHarmonics * sizeof(float));
  harmonicsPhaseCenter = (float*)fftwf_malloc(numHarmonics * sizeof(float));
  harmonicsAmpRight = (float*)fftwf_malloc(numHarmonics * sizeof(float));
  harmonicsPhaseRight = (float*)fftwf_malloc(numHarmonics * sizeof(float));
  
  freqPerBin = rate / double(size);
  //init hanningCoeff. The hanning windowing function
  hanningScalar = 0;
  for(int j=0; j<n; j++) {
    hanningCoeff[j] = (1.0 - cos(double(j+1) / double(n+1) * twoPI)) / 2.0;
    hanningScalar += hanningCoeff[j];
  }
  hanningScalar /= 2; //to normalise the FFT coefficients we divide by the sum of the hanning window / 2

  fastSmooth = new fast_smooth(n/8);
  beenInit = true;
}

void MyTransforms::Private::uninit()
{
  if(beenInit) {
    fftwf_free(harmonicsAmpLeft);
    fftwf_free(harmonicsPhaseLeft);
    fftwf_free(harmonicsAmpCenter);
    fftwf_free(harmonicsPhaseCenter);
    fftwf_free(harmonicsAmpRight);
    fftwf_free(harmonicsPhaseRight);
    fftwf_destroy_plan(planDataFFT2Time);
    fftwf_destroy_plan(planDataTime2FFT);
    fftwf_destroy_plan(planAutocorrFFT2Time);
    fftwf_destroy_plan(planAutocorrTime2FFT);
    fftwf_free(autocorrFFT);
    fftwf_free(autocorrTime);
    fftwf_free(dataFFT);
    fftwf_free(dataTime);
    fftwf_free(dataTemp);
    delete fastSmooth;
    beenInit = false;
  }
}

/** Performs an autocorrelation on the input
  @param input An array of length n, in which the autocorrelation is taken
  @param ouput This should be an array of length k.
               This is the correlation of the signal with itself
               for delays 1 to k (stored in elements 0 to k-1)
  @return The sum of squares of the input. (ie the zero delay correlation)
  Note: Use the init function to set values of n and k before calling this.
*/
double MyTransforms::Private::autocorr(float *input, float *output)
{
  myassert(beenInit);
  float fsize = float(size);
  
  //pack the data into an array which is zero padded by k elements
  std::copy(input, input+n, autocorrTime);
  std::fill(autocorrTime+n, autocorrTime+size, 0.0f);

  //Do a forward FFT
  fftwf_execute(planAutocorrTime2FFT);

  //calculate the (real*real + ima*imag) for each coefficient
  //Note: The numbers are packed in half_complex form (refer fftw)
  //ie. R[0], R[1], R[2], ... R[size/2], I[(size+1)/2+1], ... I[3], I[2], I[1]
  for(int j=1; j<size/2; j++) {
    autocorrFFT[j] = sq(autocorrFFT[j]) + sq(autocorrFFT[size-j]);
    autocorrFFT[size-j] = 0.0f;
  }
  autocorrFFT[0] = sq(autocorrFFT[0]);
  autocorrFFT[size/2] = sq(autocorrFFT[size/2]);

  //Do an inverse FFT
  fftwf_execute(planAutocorrFFT2Time);

  //extract the wanted elements out, and normalise
  //for(int x=0; x<k; x++)
  //  output[x] = autocorrTime[x+1] / fsize;
  for(float *p1=output, *p2=autocorrTime+1; p1<output+k;) {
    *p1++ = *p2++ / fsize;
  }
    
  return double(autocorrTime[0]) / double(size);
}

double MyTransforms::Private::autoLogCorr(float *input, float *output)
{
  myassert(beenInit);
  float fsize = float(size);
  
  //pack the data into an array which is zero padded by k elements
  std::copy(input, input+n, autocorrTime);
  std::fill(autocorrTime+n, autocorrTime+size, 0.0f);

  //Do a forward FFT
  fftwf_execute(planAutocorrTime2FFT);

  //calculate the (real*real + ima*imag) for each coefficient
  //Note: The numbers are packed in half_complex form (refer fftw)
  //ie. R[0], R[1], R[2], ... R[size/2], I[(size+1)/2+1], ... I[3], I[2], I[1]
  for(int j=1; j<size/2; j++) {
    autocorrFFT[j] = (sq(autocorrFFT[j]) + sq(autocorrFFT[size-j]));
    autocorrFFT[size-j] = 0.0f;
  }
  autocorrFFT[0] = (sq(autocorrFFT[0]));
  autocorrFFT[size/2] = (sq(autocorrFFT[size/2]));

  //Do an inverse FFT
  fftwf_execute(planAutocorrFFT2Time);

  //extract the wanted elements out, and normalise
  for(float *p1=output, *p2=autocorrTime+1; p1<output+k;) {
    *p1++ = *p2++ / fsize;
  }
    
  return double(autocorrTime[0]) / double(size);
}

/** The Average Square Difference Function.
    @param input. An array of length n, in which the ASDF is taken
    @param ouput. This should be an array of length k
*/
double MyTransforms::Private::asdf(float *input, float *output)
{
  myassert(beenInit);
  double sumSq = autocorr(input, output); //Do an autocorrelation and return the sum of squares of the input
  double sumRightSq = sumSq, sumLeftSq = sumSq;
  for(int j=0; j<k; j++) {
    sumLeftSq  -= sq(input[n-1-j]);
    sumRightSq -= sq(input[j]);
    output[j] = sumLeftSq + sumRightSq - 2*output[j];
  }
  return sumSq;
}

/** The Normalised Square Difference Function.
    @param input. An array of length n, in which the ASDF is taken
    @param ouput. This should be an array of length k
    @param analysisType
    @return The sum of square
*/
double MyTransforms::Private::nsdf(float *input, float *output, int analysisType)
{
  myassert(beenInit);
  double sumSq = autocorr(input, output); //the sum of squares of the input

  double totalSumSq = sumSq * 2.0;
  if(analysisType == MPM || analysisType == MPM_MODIFIED_CEPSTRUM) { //nsdf
    for(int j=0; j<k; j++) {
      totalSumSq  -= sq(input[n-1-j]) + sq(input[j]);
      //dividing by zero is very slow, so deal with it seperately
      if(totalSumSq > 0.0) {
          output[j] *= 2.0 / totalSumSq;
      } else {
          output[j] = 0.0;
      }
    }
  } else { //autocorr
    for(int j=0; j<k; j++) {
      //dividing by zero is very slow, so deal with it seperately
      if(totalSumSq > 0.0) {
          output[j] /= sumSq;
      } else {
          output[j] = 0.0;
      }
    }
  }
  return sumSq;
}

/**
    Find the highest maxima between each pair of positive zero crossings.
    Including the highest maxima between the last +ve zero crossing and the end if any.
    Ignoring the first (which is at zero)
    In this diagram the disired maxima are marked with a *

                  *             *
    \      *     /\      *     /\
    _\____/\____/__\/\__/\____/__
      \  /  \  /      \/  \  /
       \/    \/            \/

  @param input The array to look for maxima in
  @param len Then length of the input array
  @param maxPositions The resulting maxima positions are pushed back to this vector
  @return The index of the overall maximum
*/
int MyTransforms::findNSDFMaxima(float *input, int len, std::vector<int> &maxPositions)
{
  int pos = 0;
  int curMaxPos = 0;
  int overallMaxIndex = 0;

  while(pos < (len-1)/3 && input[pos] > 0.0f) pos++; //find the first negitive zero crossing
  while(pos < len-1 && input[pos] <= 0.0f) pos++; //loop over all the values below zero
  if(pos == 0) pos = 1; // can happen if output[0] is NAN
  
  while(pos < len-1) {
    myassert(!(input[pos] < 0)); //don't assert on NAN
    if(input[pos] > input[pos-1] && input[pos] >= input[pos+1]) { //a local maxima
      if(curMaxPos == 0) curMaxPos = pos; //the first maxima (between zero crossings)
      else if(input[pos] > input[curMaxPos]) curMaxPos = pos; //a higher maxima (between the zero crossings)
    }
    pos++;
    if(pos < len-1 && input[pos] <= 0.0f) { //a negative zero crossing
      if(curMaxPos > 0) { //if there was a maximum
        maxPositions.push_back(curMaxPos); //add it to the vector of maxima
        if(overallMaxIndex == 0) overallMaxIndex = curMaxPos;
        else if(input[curMaxPos] > input[overallMaxIndex]) overallMaxIndex = curMaxPos;
        curMaxPos = 0; //clear the maximum position, so we start looking for a new ones
      }
      while(pos < len-1 && input[pos] <= 0.0f) pos++; //loop over all the values below zero
    }
  }

  if(curMaxPos > 0) { //if there was a maximum in the last part
    maxPositions.push_back(curMaxPos); //add it to the vector of maxima
    if(overallMaxIndex == 0) overallMaxIndex = curMaxPos;
    else if(input[curMaxPos] > input[overallMaxIndex]) overallMaxIndex = curMaxPos;
    curMaxPos = 0; //clear the maximum position, so we start looking for a new ones
  }
  return overallMaxIndex;
}

/** @return The index of the first sub maximum.
  This is now scaled from (threshold * overallMax) to 0.
*/
int MyTransforms::Private::findNSDFsubMaximum(float *input, int len, float threshold)
{
  std::vector<int> indices;
  int overallMaxIndex = findNSDFMaxima(input, len, indices);
  threshold += (1.0 - threshold) * (1.0 - input[overallMaxIndex]);
  float cutoff = input[overallMaxIndex] * threshold;
  for(uint j=0; j<indices.size(); j++) {
    if(input[indices[j]] >= cutoff) return indices[j];
  }
  myassert(0); //should never get here
  return 0; //stop the compiler warning
}

/** Estimate the period or the fundamental frequency.
  Finds the first maximum of NSDF which past the first
  positive zero crossing and is over the threshold.
  If no maxima are over the threshold then the the highest maximum is returned.
  If no positive zero crossing is found, zero is returned.
  @param input. An array of length n.
  @param threshold. A number between 0 and 1 at which maxima above are acceped.
  @param analysisType
  @param ampThresholds
  @return The estimated period (in samples), or zero if none found.
*/
void MyTransforms::calculateAnalysisData(int chunk, ChannelBase *ch,
                                         int analysisType,
                                         double ampThresholds[NUM_AMP_MODES][2],
                                         bool doingFreqAnalysis,
                                         bool doingActiveAnalysis,
                                         bool doingAutoNoiseFloor,
                                         bool doingHarmonicAnalysis,
                                         double& rmsFloor,
                                         double& rmsCeiling,
                                         double topPitch,
                                         double noteThreshold)
{
    d->calculateAnalysisData(chunk, ch, analysisType,
                             ampThresholds, doingFreqAnalysis,
                             doingActiveAnalysis, doingAutoNoiseFloor,
                             doingHarmonicAnalysis, rmsFloor, rmsCeiling,
                             topPitch, noteThreshold);
}

void MyTransforms::Private::calculateAnalysisData(int chunk, ChannelBase *ch,
                                         int analysisType,
                                         double ampThresholds[NUM_AMP_MODES][2],
                                         bool doingFreqAnalysis,
                                         bool doingActiveAnalysis,
                                         bool doingAutoNoiseFloor,
                                         bool doingHarmonicAnalysis,
                                         double& rmsFloor,
                                         double& rmsCeiling,
                                         double topPitch,
                                         double noteThreshold)
{
  const double ampThreshold = ampThresholds[AMPLITUDE_RMS][0];
  myassert(ch);
  myassert(ch->dataAtChunk(chunk));
  AnalysisData &analysisData = *ch->dataAtChunk(chunk);
  AnalysisData *prevAnalysisData = ch->dataAtChunk(chunk-1);
  float *output = ch->nsdfData.begin();
  float *curInput = (equalLoudness) ? ch->filteredInput.begin() : ch->directInput.begin();

  std::vector<int> nsdfMaxPositions;

  analysisData.maxIntensityDB() = linear2dB(fabs(*std::max_element(curInput, curInput+n, absoluteLess<float>())), ch->dBFloor());
  
  doChannelDataFFT(ch, curInput, chunk, analysisType);
  std::copy(curInput, curInput+n, dataTime);
  
  if(doingFreqAnalysis && (ch->firstTimeThrough() || doingActiveAnalysis)) {
    //calculate the Normalised Square Difference Function
    double logrms = linear2dB(nsdf(dataTime, ch->nsdfData.begin(), analysisType) / double(n), ch->dBFloor()); /**< Do the NSDF calculation */
    analysisData.logrms() = logrms;
    if(doingAutoNoiseFloor && !analysisData.done) {
      //do it for gdata. this is only here for old code. remove some stage
      if(chunk == 0) { rmsFloor = 0.0; rmsCeiling = ch->dBFloor(); }
      if(logrms+15 < rmsFloor) rmsFloor = logrms+15;
      if(logrms > rmsCeiling) rmsCeiling = logrms;

      //do it for the channel
      if(chunk == 0) { ch->rmsFloor = 0.0; ch->rmsCeiling = ch->dBFloor(); }
      if(logrms+15 < ch->rmsFloor) ch->rmsFloor = logrms+15;
      if(logrms > ch->rmsCeiling) ch->rmsCeiling = logrms;
    }

    //analysisData.freqCentroid() = calcFreqCentroid(storeFFT, size);
    analysisData.freqCentroid() = calcFreqCentroidFromLogMagnitudes(ch->fftData1.begin(), ch->fftData1.size());
    if(prevAnalysisData)
      analysisData.deltaFreqCentroid() = bound(fabs(analysisData.freqCentroid() - prevAnalysisData->freqCentroid())*20.0, 0.0, 1.0);
    else 
      analysisData.deltaFreqCentroid() = 0.0;
    
    findNSDFMaxima(ch->nsdfData.begin(), k, nsdfMaxPositions);

    //store some of the best period estimates
    analysisData.periodEstimates.clear();
    analysisData.periodEstimatesAmp.clear();
    float smallCutoff = 0.4f;
    for(std::vector<int>::iterator iter = nsdfMaxPositions.begin(); iter < nsdfMaxPositions.end(); iter++) {
      if(output[*iter] >= smallCutoff) {
        float x, y;
        //do a parabola fit to find the maximum
        parabolaTurningPoint2(output[*iter-1], output[*iter], output[*iter+1], float(*iter + 1), &x, &y);
        y = bound(y, -1.0f, 1.0f);
        analysisData.periodEstimates.push_back(x);
        analysisData.periodEstimatesAmp.push_back(y);
      }
    }
    
    float periodDiff = 0.0f;
    //if(maxPositions.empty()) { //no period found
    if(analysisData.periodEstimates.empty()) { //no period found
      //analysisData.correlation() = 0.0f;
      analysisData.calcScores(ampThresholds);
      analysisData.done = true;
      //goto finished; //return;
    } else {
      //calc the periodDiff
      if(chunk > 0 && prevAnalysisData->highestCorrelationIndex!=-1) {
        float prevPeriod = prevAnalysisData->periodEstimates[prevAnalysisData->highestCorrelationIndex];
        std::vector<float>::iterator closestIter = binary_search_closest(analysisData.periodEstimates.begin(), analysisData.periodEstimates.end(), prevPeriod);
        periodDiff = *closestIter - prevPeriod;
        if(absolute(periodDiff) > 8.0f) periodDiff = 0.0f;
      }

      int nsdfMaxIndex = int(std::max_element(analysisData.periodEstimatesAmp.begin(), analysisData.periodEstimatesAmp.end()) - analysisData.periodEstimatesAmp.begin());
      analysisData.highestCorrelationIndex = nsdfMaxIndex;

      if(!analysisData.done) {
        if(analysisType == MPM_MODIFIED_CEPSTRUM) {
            ch->chooseCorrelationIndex(chunk, float(analysisData.cepstrumIndex), analysisType, topPitch); //calculate pitch
        } else {
          if(ch->isNotePlaying() && chunk > 0) {
            ch->chooseCorrelationIndex(chunk, ch->periodOctaveEstimate(chunk-1), analysisType, topPitch); //calculate pitch
          } else {
            ch->chooseCorrelationIndex1(chunk, topPitch); //calculate pitch
          }
        }
        ch->calcDeviation(chunk);

        ch->doPronyFit(chunk); //calculate vibratoPitch, vibratoWidth, vibratoSpeed
      }

      analysisData.changeness() = 0.0f;

      if(doingHarmonicAnalysis) {
        std::copy(dataTime, dataTime+n, dataTemp);
        if(analysisData.chosenCorrelationIndex >= 0)
          doHarmonicAnalysis(dataTemp, analysisData, analysisData.periodEstimates[analysisData.chosenCorrelationIndex]/*period*/, ampThreshold);
      }
    }

    if(doingFreqAnalysis && ch->doingDetailedPitch() && ch->firstTimeThrough()) {
      float periodDiff2 = ch->calcDetailedPitch(curInput, analysisData.period, chunk, topPitch);
      periodDiff = periodDiff2;
      ch->pitchLookup.insert(ch->pitchLookup.end(), ch->detailedPitchData.begin(),
                             ch->detailedPitchData.end());
      ch->pitchLookupSmoothed.insert(ch->pitchLookupSmoothed.end(),
                                     ch->detailedPitchDataSmoothed.begin(),
                                     ch->detailedPitchDataSmoothed.end());
    }

    if(!analysisData.done) {
      analysisData.calcScores(ampThresholds);
      ch->processNoteDecisions(chunk, periodDiff, analysisType, topPitch, noteThreshold);
      analysisData.done = true;
    }

    if(doingFreqAnalysis && ch->doingDetailedPitch() && ch->firstTimeThrough()) {
      ch->calcVibratoData(chunk);
    }
  }

  if(doingFreqAnalysis && ch->doingDetailedPitch() && (!ch->firstTimeThrough())) {
    std::copy(ch->pitchLookup.begin(), ch->pitchLookup.end(),
              ch->detailedPitchData.begin());
    std::copy(ch->pitchLookupSmoothed.begin(), ch->pitchLookupSmoothed.end(),
              ch->detailedPitchDataSmoothed.begin());
  }

  if(!analysisData.done) {
    int j;
    //calc rms by hand
    double rms = 0.0;
    for(j=0; j<n; j++) {
      rms += sq(dataTime[j]);
    }
    analysisData.logrms() = linear2dB(rms / float(n), ch->dBFloor());
    analysisData.calcScores(ampThresholds);
    analysisData.done = true;
  }
}

/* using the given fractional period, constructs a smaller subwindow
which slide through the window. The smallest clarity measure of this
moving sub window is returned.*/
float MyTransforms::Private::get_fine_clarity_measure(double period)
{
    int tempN = n - int(ceil(period));
    float *tempData = new float[tempN];
    float bigSum=0, corrSum=0, matchVal, matchMin=1.0;
    //create some data points at the fractional period delay into tempData
    stretch_array(n, dataTime, tempN, tempData, period, tempN, LINEAR);
    int dN = int(floor(period)); //tempN / 4;
    for(int j=0; j<dN; j++) {
      bigSum += sq(dataTime[j]) + sq(tempData[j]);
      corrSum += dataTime[j] * tempData[j];
    }
    matchMin = (2.0*corrSum) / bigSum;
    for(int j=0; j<tempN - dN; j++) {
      bigSum -= sq(dataTime[j]) + sq(tempData[j]);
      corrSum -= dataTime[j] * tempData[j];
      bigSum += sq(dataTime[j+dN]) + sq(tempData[j+dN]);
      corrSum += dataTime[j+dN] * tempData[j+dN];
      matchVal = (2.0*corrSum) / bigSum;
      if(matchVal < matchMin) matchMin = matchVal;
    }
    delete[] tempData;

  return matchMin;
}

double MyTransforms::Private::get_max_note_change(float *input, double period)
{
  //TODO
/*matlab code
l = length(s);
m = floor(l / 4); % m is the maximum size sub-window to use
% w is the sub-window size
if p < m
    w = p * floor(m / p);
else
    w = p;
end

n = -4:4;
ln = length(n);
left = cell(1, ln);
right = cell(1, ln);
left_pow = zeros(1, ln);
right_pow = zeros(1, ln);
pow = zeros(1, ln);
err = zeros(1, ln);

for i = 1:ln
    left{i} = s(1:(w-n(i))); % a smaller delay period has a larger window size, to ensure only the same data is used
    right{i} = s(1+p+n(i):w+p);
    left_pow(i) = sum(left{i}.^2);
    right_pow(i) = sum(right{i}.^2);
    err(i) = sum((left{i} - right{i}) .^ 2);
end
*/
  int i, j, j2;
  int max_subwindow = n / 4;
  int subwindow_size;
  if(period < 1.0) return 0.0; //period too small
  if(period > n/2) { printf("period = %f\n", period); return 0.0; }
  int iPeriod = int(floor(period));
  if(period < double(max_subwindow)) subwindow_size = int(floor(period * (double(max_subwindow) / period)));
  else subwindow_size = int(floor(period));
  int num = n-subwindow_size-iPeriod;

  std::vector<int> offsets;
  for(j=-4; j<=4; j++) offsets.push_back(j); //do a total of 9 subwindows at once. 4 either side.
  int ln = offsets.size();

  std::vector<float> left(ln);
  std::vector<float> right(ln);
  std::vector<float> left_pow(ln);
  std::vector<float> right_pow(ln);
  std::vector<float> pow(ln);
  std::vector<float> err(ln);
  std::vector<float> result(ln);
  std::vector<float> unsmoothed(num);
  std::vector<float> smoothed(num);
  std::vector<float> smoothed_diff(num);
  //calc the values of pow and err for the first in each row.
  for(i=0; i<ln; i++) {
    left_pow[i] = right_pow[i] = pow[i] = err[i] = 0;
    for(j=0, j2=iPeriod+offsets[i]; j<subwindow_size-offsets[i]; j++, j2++) {
      left_pow[i] += sq(input[j]);
      right_pow[i] += sq(input[j2]);
      err[i] += sq(input[j] - input[j2]);
    }
  }
  //printf("subwindow_size=%d, num=%d, period=%lf\n", subwindow_size, num, period);
/*matlab code
for j = 1:(num-1)
    for i = 1:ln
        pow(i) = (left_pow(i) + right_pow(i));
        resulte(i, j) = err(i);
        resultp(i, j) = pow(i);
        result(i, j) = 1 - (err(i) / pow(i));

        %err = err - (s(j) - s(j+p)).^2 + (s(j+w) - s(j+w+p)).^2;
        err(i) = err(i) - ((s(j) - s(j+p+n(i))).^2) + (s(j+w-n(i)) - s(j+w+p)).^2;
        left_pow(i) = left_pow(i) - s(j).^2 + s(j+w-n(i)).^2;
        right_pow(i) = right_pow(i) - s(j+p+n(i)).^2 + s(j+p+w).^2;
    end
end

for i = 1:ln
    pow(i) = (left_pow(i) + right_pow(i));
    result(i, num) = 1 - (err(i) / pow(i));
end
*/
  //TODO: speed up this for loop
  for(j=0; j<num-1; j++) {
    for(i=0; i<ln; i++) {
      pow[i] = left_pow[i] + right_pow[i];
      result[i] = 1.0 - (err[i] / pow[i]);

      err[i] += -sq(input[j] - input[j+iPeriod+offsets[i]]) + sq(input[j+subwindow_size-offsets[i]] - input[j+subwindow_size+iPeriod]);
      left_pow[i] += -sq(input[j]) + sq(input[j+subwindow_size-offsets[i]]);
      right_pow[i] += -sq(input[j+iPeriod+offsets[i]]) + sq(input[j+iPeriod+subwindow_size]);
    }
/*matlab code
for j = 1:num
    [dud pos] = max(result(:,j));
    if pos > 1 && pos < ln
        [period(j) dummy] = parabolaPeak(result(pos-1, j), result(pos, j), result(pos+1, j), p+n(pos));
    else
        period(j) = p+n(pos);
    end
    period_int(j) = p+n(pos);
end*/
    int pos = int(std::max_element(result.begin(), result.begin()+ln) - result.begin());
    if(pos > 0 && pos < ln-1)
      unsmoothed[j] = double(iPeriod + offsets[pos]) + parabolaTurningPoint(result[pos-1], result[pos], result[pos+1]);
    else
      unsmoothed[j] = double(iPeriod + offsets[pos]);
  }
  fastSmooth->fast_smoothB(&(unsmoothed[0]), &(smoothed[0]), num-1);
  int max_pos = 0;
  for(j=0; j<num-2; j++) {
    smoothed_diff[j] = fabs(smoothed[j+1] - smoothed[j]);
    if(smoothed_diff[j] > smoothed_diff[max_pos]) max_pos = j;
  }
  double ret = smoothed_diff[max_pos] / period * double(rate) * double(subwindow_size) / 10000.0;
  return ret;
}

/** Builds a lookup table for use in doEqualLoudness
  @param dB The decibel level of at which the human ear filter should be simulated (default 50dB)
*/

/** Find the index of the first maxima above the threshold times the overall maximum.
  @param threshold A value between 0.0 and 1.0
  @return The index of the first subMaxima
*/
int findFirstSubMaximum(float *data, int length, float threshold)
{
  float maxValue = *std::max_element(data, data+length);
  float cutoffValue = maxValue * threshold;
  for(int j=0; j < length; j++) {
    if(data[j] >= cutoffValue) return j;
  }
  myassert(0); //shouldn't get here.
  return length;
}

/** Given cepstrum input finds the first maxima above the threshold
    @param threshold The fraction of the 'overall maximum value' the 'cepstrum maximum' must be over.
                     A value between 0.0 and 1.0.
*/
int findCepstrumMaximum(float *data, int length, float threshold)
{
  int pos = 0;
  //loop until the first negative value in data
  while(pos < length-1 && data[pos] > 0.0f) pos++;
  return pos + findFirstSubMaximum(data+pos, length-pos, threshold);
}

/** Do a Windowed FFT for use in the Active FFT data window
*/
void MyTransforms::Private::doChannelDataFFT(ChannelBase *ch, float *curInput, int chunk, int analysisType)
{
  std::copy(curInput, curInput+n, dataTime);
  applyHanningWindow(dataTime);
  fftwf_execute(planDataTime2FFT);
  int nDiv2 = n/2;
  //LOG RULES: log(sqrt(x)) = log(x) / 2.0
  //LOG RULES: log(a * b) = log(a) + log(b)
  myassert(ch->fftData1.size() == nDiv2);
  double logSize = log10(double(ch->fftData1.size())); //0.0
  //Adjust the coefficents, both real and imaginary part by same amount
  double sqValue;
  const double logBase = 100.0;
  for(int j=1; j<nDiv2; j++) {
    sqValue = sq(dataFFT[j]) + sq(dataFFT[n-j]);
    ch->fftData2[j] = logBaseN(logBase, 1.0 + 2.0*sqrt(sqValue) / double(nDiv2) * (logBase-1.0));
    if(sqValue > 0.0) {
      ch->fftData1[j] = bound(log10(sqValue) / 2.0 - logSize, ch->dBFloor(), 0.0);
    } else {
        ch->fftData1[j] = ch->dBFloor();
    }
  }
  sqValue = sq(dataFFT[0]) + sq(dataFFT[nDiv2]);
  ch->fftData2[0] = logBaseN(logBase, 1.0 + 2.0*sqrt(sqValue) / double(nDiv2) * (logBase-1.0));
  if(sqValue > 0.0) {
    ch->fftData1[0] = bound(log10(sqValue) / 2.0 - logSize, ch->dBFloor(), 0.0);
  } else {
    ch->fftData1[0] = ch->dBFloor();
  }

  if(analysisType == MPM_MODIFIED_CEPSTRUM) {
    for(int j=1; j<nDiv2; j++) {
      dataFFT[j] = ch->fftData2[j];
      dataFFT[n-j] = 0.0;
    }
    dataFFT[0] = ch->fftData2[0];
    dataFFT[nDiv2] = 0.0;
    fftwf_execute(planDataFFT2Time);

    for(int j=1; j<n; j++) {
      dataTime[j] /= dataTime[0];
    }
    dataTime[0] = 1.0;
    for(int j=0; j<nDiv2; j++) ch->cepstrumData[j] = dataTime[j];
    AnalysisData &analysisData = *ch->dataAtChunk(chunk);
    analysisData.cepstrumIndex = findNSDFsubMaximum(dataTime, nDiv2, 0.6f);
    analysisData.cepstrumPitch = freq2pitch(double(analysisData.cepstrumIndex) / ch->rate());
  }
}

void MyTransforms::Private::calcHarmonicAmpPhase(float *harmonicsAmp, float *harmonicsPhase, int binsPerHarmonic)
{
  int harmonic;
  for(int j=0; j<numHarmonics; j++) {
    harmonic = (j+1) * binsPerHarmonic;
    if(harmonic < n) {
      harmonicsAmp[j] = sqrt(sq(dataFFT[harmonic]) + sq(dataFFT[n-harmonic]));
      harmonicsPhase[j] = atan2f(dataFFT[n-harmonic], dataFFT[harmonic]);
    } else {
      harmonicsAmp[j] = 0.0;
      harmonicsPhase[j] = 0.0;
    }
  }
}

void MyTransforms::Private::doHarmonicAnalysis(float *input, AnalysisData &analysisData, double period, double ampThreshold)
{
  double numPeriodsFit = floor(double(n) / period); //2.0
  double numPeriodsUse = numPeriodsFit - 1.0;
  int iNumPeriodsUse = int(numPeriodsUse);
  double centerX = float(n) / 2.0;

  //do left
  double start = centerX - (numPeriodsFit / 2.0) * period;
  double length = (numPeriodsUse) * period;
  stretch_array(n, input, n, dataTime, start, length, LINEAR);
  applyHanningWindow(dataTime);
  fftwf_execute(planDataTime2FFT);
  calcHarmonicAmpPhase(harmonicsAmpLeft, harmonicsPhaseLeft, iNumPeriodsUse);
  
  //do center
  start += period / 2.0;
  stretch_array(n, input, n, dataTime, start, length, LINEAR);
  applyHanningWindow(dataTime);
  fftwf_execute(planDataTime2FFT);
  calcHarmonicAmpPhase(harmonicsAmpCenter, harmonicsPhaseCenter, iNumPeriodsUse);
  
  //do right
  start += period / 2.0;
  stretch_array(n, input, n, dataTime, start, length, LINEAR);
  applyHanningWindow(dataTime);
  fftwf_execute(planDataTime2FFT);
  calcHarmonicAmpPhase(harmonicsAmpRight, harmonicsPhaseRight, iNumPeriodsUse);
  
  double freq = rate / period;
  
  analysisData.harmonicAmpNoCutOff.resize(numHarmonics);
  analysisData.harmonicAmp.resize(numHarmonics);
  analysisData.harmonicFreq.resize(numHarmonics);
  analysisData.harmonicNoise.resize(numHarmonics);

  for(int j=0; j<numHarmonics; j++) {
    analysisData.harmonicAmpNoCutOff[j] = analysisData.harmonicAmp[j] = log10(harmonicsAmpCenter[j] / hanningScalar) * 20;
    analysisData.harmonicAmp[j] = 1.0 - (analysisData.harmonicAmp[j] / ampThreshold);
    if(analysisData.harmonicAmp[j] < 0.0) analysisData.harmonicAmp[j] = 0.0;
    //should be 1 whole period between left and right. i.e. the same freq give 0 phase difference
    double diffAngle = (harmonicsPhaseRight[j] - harmonicsPhaseLeft[j]) / twoPI;
    diffAngle = cycle(diffAngle + 0.5, 1.0) - 0.5;
    double diffAngle2 = (harmonicsPhaseCenter[j] - harmonicsPhaseLeft[j]) / twoPI;
    //if an odd harmonic the phase will be 180 degrees out. So correct for this
    if(j % 2 == 0) diffAngle2 += 0.5;
    diffAngle2 = cycle(diffAngle2 + 0.5, 1.0) - 0.5;
    analysisData.harmonicNoise[j] = fabs(diffAngle2 - diffAngle);
    analysisData.harmonicFreq[j] = float(freq * double(j+1)) + (freq*diffAngle);
  }
}

void MyTransforms::Private::applyHanningWindow(float *d)
{
  for(int j=0; j<n; j++) d[j] *= hanningCoeff[j];
}

/**
  @param buffer The input buffer, as outputted from the fftw. ie 1st half real, 2nd half imaginary parts
  @param len The length of the buffer, including real and imaganary parts
  @return The normalised frequency centriod
*/
double MyTransforms::Private::calcFreqCentroid(float *buffer, int len)
{
  double centroid = 0.0;
  double totalWeight = 0.0;
  double absValue;
  for(int j=1; j<len/2; j++) { //ignore the end freq bins, ie j=0
    //calculate centroid
    absValue = sqrt(sq(buffer[j]) + sq(buffer[len-j]));
    centroid += double(j)*absValue;
    totalWeight += absValue;
  }
  centroid /= totalWeight * double(len/2);
  return centroid;
}

/**
  @param buffer The input buffer of logarithmic magnitudes
  @param len The length of the buffer
  @return The normalised frequency centriod
*/
double MyTransforms::Private::calcFreqCentroidFromLogMagnitudes(float *buffer, int len)
{
  double centroid = 0.0;
  double totalWeight = 0.0;
  for(int j=1; j<len; j++) { //ignore the end freq bins, ie j=0
    //calculate centroid
    centroid += double(j)*buffer[j];
    totalWeight += buffer[j];
  }
  return centroid;
}
