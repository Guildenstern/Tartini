#include "channelbase.h"
#include "musicnotes.h"
#include "prony.h"
#include "FastSmoothedAveragingFilter.h"
#include "GrowingAveragingFilter.h"
#include "bspline.h"
#include "myassert.h"
#include "analysisdata.h"
#include "notedata.h"
#include "mytransforms.h"

const double ChannelBase::shortTime = 0.08; //0.08; //0.18;
const float shortBase = 0.1f;
const float shortStretch = 1.5; //1.0f; //3.5f;

const double ChannelBase::longTime = 0.8;
const float longBase = 0.02f; //0.1f;
const float longStretch = 0.2f; //0.3f; //1.0f;

ChannelBase::ChannelBase(double timeperchunk_, int sampleRate_,
                         int framesperchunk_, int size, int k) : lookup(),
        timeperchunk(timeperchunk_),
        sampleRate(sampleRate_),
        framesperchunk(framesperchunk_) {
    k = (k) ?k :(size + 1) / 2;
    pitchSmallSmoothingFilter = new GrowingAverageFilter(sampleRate/64);
    pitchBigSmoothingFilter = new FastSmoothedAveragingFilter(sampleRate/16);
    nsdfAggregateData.resize(k, 0.0);
    nsdfAggregateDataScaled.resize(k, 0.0);
    directInput.resize(size, 0.0);
    filteredInput.resize(size, 0.0);
    nsdfData.resize(k, 0.0);
    nsdfAggregateRoof = 0.0;
    fftData1.resize(size/2, 0.0);
    fftData2.resize(size/2, 0.0);
    cepstrumData.resize(size/2, 0.0);
    detailedPitchData.resize(size/2, 0.0);
    detailedPitchDataSmoothed.resize(size/2, 0.0);
    pronyWindowSize = int(ceil(0.4/timePerChunk()));
    pronyData.resize(pronyWindowSize);
    noteIsPlaying = false;

    std::deque<float> b = pitchLookupSmoothed;
}

ChannelBase::~ChannelBase() {
    delete pitchSmallSmoothingFilter;
    delete pitchBigSmoothingFilter;
}

/** Choose the correlation index (with no starting octave estimate)
  * For use with at the start of the note
  */
void ChannelBase::chooseCorrelationIndex1(int chunk, double topPitch)
{
  myassert(dataAtChunk(chunk));
  AnalysisData &analysisData = *dataAtChunk(chunk);
  uint iterPos;
  int choosenMaxIndex = 0;
  if(analysisData.periodEstimates.empty()) { //no period found
    return;
  }
  //choose a cutoff value based on the highest value and a relative threshold
  float highest = analysisData.periodEstimatesAmp[analysisData.highestCorrelationIndex];
  float cutoff = highest * threshold();
  //find the first of the maxPositions which is above the cutoff
  for(iterPos = 0; iterPos < analysisData.periodEstimatesAmp.size(); iterPos++) {
    if(analysisData.periodEstimatesAmp[iterPos] >= cutoff) { choosenMaxIndex = iterPos; break; }
  }
  analysisData.chosenCorrelationIndex = choosenMaxIndex;
  analysisData.correlation() = analysisData.periodEstimatesAmp[choosenMaxIndex];

  analysisData.period = analysisData.periodEstimates[choosenMaxIndex];
  double freq = rate() / analysisData.period;
  analysisData.fundamentalFreq = float(freq);
  analysisData.pitch = bound(freq2pitch(freq), 0.0, topPitch);
  analysisData.pitchSum = (double)analysisData.pitch;
  analysisData.pitch2Sum = sq((double)analysisData.pitch);
}


/** This uses an octave extimate to help chose the correct correlation index
  * Returns true if the new correlation index is different from the old one
  */
bool ChannelBase::chooseCorrelationIndex(int chunk, float periodOctaveEstimate,
                                         int analysisType, double topPitch)
{
  myassert(dataAtChunk(chunk));
  AnalysisData &analysisData = *dataAtChunk(chunk);
  uint iterPos;
  int choosenMaxIndex = 0;
  bool isDifferentIndex = false;
  if(analysisData.periodEstimates.empty()) { //no period found
    return false;
  }
  if(analysisType == MPM || analysisType == MPM_MODIFIED_CEPSTRUM) {
  //choose the periodEstimate which is closest to the periodOctaveEstimate
    float minDist = fabs(analysisData.periodEstimates[0] - periodOctaveEstimate);
    float dist;
    for(iterPos = 1; iterPos < analysisData.periodEstimates.size(); iterPos++) {
      dist = fabs(analysisData.periodEstimates[iterPos] - periodOctaveEstimate);
      if(dist < minDist) { minDist = dist; choosenMaxIndex = iterPos; }
    }
  } else {
    choosenMaxIndex = analysisData.highestCorrelationIndex;
  }
  if(choosenMaxIndex != analysisData.chosenCorrelationIndex) isDifferentIndex = true;
  analysisData.chosenCorrelationIndex = choosenMaxIndex;
  analysisData.correlation() = analysisData.periodEstimatesAmp[choosenMaxIndex];

  analysisData.period = analysisData.periodEstimates[choosenMaxIndex];
  double freq = rate() / analysisData.period;
  analysisData.fundamentalFreq = float(freq);
  analysisData.pitch = bound(freq2pitch(freq), 0.0, topPitch);
  if(chunk > 0 && !isFirstChunkInNote(chunk)) {
    analysisData.pitchSum = dataAtChunk(chunk-1)->pitchSum + (double)analysisData.pitch;
    analysisData.pitch2Sum = dataAtChunk(chunk-1)->pitch2Sum + sq((double)analysisData.pitch);
  } else {
    analysisData.pitchSum = (double)analysisData.pitch;
    analysisData.pitch2Sum = sq((double)analysisData.pitch);
  }
  return isDifferentIndex;
}

bool ChannelBase::isFirstChunkInNote(int chunk)
{
  AnalysisData *analysisData = dataAtChunk(chunk);
  if(analysisData && analysisData->noteIndex >= 0 && noteData[analysisData->noteIndex].startChunk() == chunk) return true;
  else return false;
}


float ChannelBase::periodOctaveEstimate(int chunk)
{
  AnalysisData *analysisData = dataAtChunk(chunk);
  if(analysisData && analysisData->noteIndex >= 0) {
    return noteData[analysisData->noteIndex].periodOctaveEstimate() * analysisData->periodRatio;
  }
  else return -1.0f;
}

void ChannelBase::calcDeviation(int chunk) {
  int lastChunk = chunk;
  int currentNoteIndex = getCurrentNoteIndex();
  myassert(dataAtChunk(chunk));
  AnalysisData &lastChunkData = *dataAtChunk(lastChunk);
  if(currentNoteIndex < 0) return;

  //Do long term calculation
  int firstChunk = MAX(lastChunk - (int)ceil(longTime/timePerChunk()), noteData[currentNoteIndex].startChunk());
  AnalysisData *firstChunkData = dataAtChunk(firstChunk);
  int numChunks = (lastChunk - firstChunk);
  double mean_sum, mean, sumX2, variance, standard_deviation;
  if(numChunks > 0) {
    mean_sum = (lastChunkData.pitchSum - firstChunkData->pitchSum);
    mean = mean_sum / double(numChunks);
    lastChunkData.longTermMean = mean;
    sumX2 = (lastChunkData.pitch2Sum - firstChunkData->pitch2Sum);
    variance = sumX2 / double(numChunks) - sq(mean);
    standard_deviation = sqrt(fabs(variance));
    lastChunkData.longTermDeviation = longBase + sqrt(standard_deviation)*longStretch;
  } else {
    lastChunkData.longTermMean = firstChunkData->pitch;
    lastChunkData.longTermDeviation = longBase;
  }

  //Do short term calculation
  firstChunk = MAX(lastChunk - (int)ceil(shortTime/timePerChunk()), noteData[currentNoteIndex].startChunk());
  firstChunkData = dataAtChunk(firstChunk);
  numChunks = (lastChunk - firstChunk);
  if(numChunks > 0) {
    mean_sum = (lastChunkData.pitchSum - firstChunkData->pitchSum);
    mean = mean_sum / double(numChunks);
    lastChunkData.shortTermMean = mean;
    sumX2 = (lastChunkData.pitch2Sum - firstChunkData->pitch2Sum);
    variance = sumX2 / double(numChunks) - sq(mean);
    standard_deviation = sqrt(fabs(variance));
    lastChunkData.shortTermDeviation = shortBase + sqrt(standard_deviation)*shortStretch;
  } else {
    lastChunkData.shortTermMean = firstChunkData->pitch;
    lastChunkData.shortTermDeviation = shortBase;
  }
}

void ChannelBase::doPronyFit(int chunk)
{
  if(chunk < pronyWindowSize) return;

  int start = chunk - pronyWindowSize;
  int center = chunk - pronyDelay();
  AnalysisData *data = dataAtChunk(center);
  for(int j=0; j<pronyWindowSize; j++) {
    pronyData[j] = dataAtChunk(start + j)->pitch;
  }
  PronyData p;
  if(pronyFit(&p, pronyData.begin(), pronyWindowSize, 2, true)) {
    data->vibratoError = p.error;
    data->vibratoPitch = p.yOffset;
    if(p.error < 1.0) {
      data->vibratoSpeed = p.freqHz(timePerChunk());
      if(p.omega * pronyWindowSize < TwoPi) {
        data->vibratoPitch = data->pitch;
      } else {
        data->vibratoWidth = p.amp;
        data->vibratoPhase = p.phase;
        data->vibratoWidthAdjust = 0.0f;
      }
    } else {
      data->vibratoPitch = data->pitch;
    }
  } else {
    data->vibratoPitch = data->pitch;
  }
}

/** Calculate (the middle half of) pitches within the current window of the input
    Calculates pitches for positions 1/4 of size() to 3/4 of size()
    e.g. for size() == 1024, does indexs 256 through < 768
    @param period The period estimate
    @return The change in period size
*/
float ChannelBase::calcDetailedPitch(float *input, double period, int /*chunk*/, float topPitch)
{
  //printf("begin calcDetailedPitch\n"); fflush(stdout);
  const int pitchSamplesRange = 4; //look 4 samples either side of the period. Total of 9 different subwindows.

  int n = size();
  int i, j, j2;
  int subwindow_size;
  if(period < pitchSamplesRange || period > (double)(n)*(3.0/8.0) - pitchSamplesRange) {
    std::fill(detailedPitchData.begin(), detailedPitchData.end(), 0.0f); //invalid period
    std::fill(detailedPitchDataSmoothed.begin(), detailedPitchDataSmoothed.end(), 0.0f); //invalid period
    return 0.0f;
  }
  int iPeriod = int(floor(period));
  subwindow_size = n/4;
  int num = n/2;
  if(iPeriod > subwindow_size) {
    subwindow_size = n/4 - (iPeriod - n/4);
  }

  std::vector<int> periods;
  for(j=-pitchSamplesRange; j<=pitchSamplesRange; j++) periods.push_back(iPeriod+j);
  int ln = periods.size();

  std::vector<float> squareTable(n);
  for(j=0; j<n; j++) squareTable[j] = sq(input[j]);

  std::vector<float> left_pow(ln);
  std::vector<float> right_pow(ln);
  std::vector<float> pow(ln);
  std::vector<float> err(ln);
  std::vector<float> result(ln);
  Array<float> unsmoothed;
  unsmoothed.resize(num);

  //calc the values of pow and err for the first in each row.
  for(i=0; i<ln; i++) {
    left_pow[i] = right_pow[i] = pow[i] = err[i] = 0;
    int offset = periods[i]-iPeriod;
    for(j=0, j2=periods[i]; j<subwindow_size-offset; j++, j2++) {
      left_pow[i] += squareTable[j];
      right_pow[i] += squareTable[j2];
      err[i] += sq(input[j] - input[j2]);
    }
  }
  //TODO: speed up this for loop
  //for(j=0; j<num-1; j++) {
  int left1 = 0;
  int left2;
  int right1;
  int right2 = left1 + subwindow_size + iPeriod;

  for(; left1<num; left1++, right2++) {
    for(i=0; i<ln; i++) {
      right1 = left1+periods[i];
      left2 = right2 - periods[i];

      pow[i] = left_pow[i] + right_pow[i];
      result[i] = 1.0 - (err[i] / pow[i]);

      err[i] += sq(input[left2] - input[right2]) - sq(input[left1] - input[right1]);
      left_pow[i]  += squareTable[left2]  - squareTable[left1];
      right_pow[i] += squareTable[right2] - squareTable[right1];
    }

    int pos = int(std::max_element(result.begin(), result.begin()+ln) - result.begin());
    myassert(pos >= 0 && pos < ln);
    if(pos > 0 && pos < ln-1)
      unsmoothed[left1] = double(periods[pos]) + parabolaTurningPoint(result[pos-1], result[pos], result[pos+1]);
    else
      unsmoothed[left1] = double(periods[pos]);
  }

  float periodDiff = unsmoothed.back() - unsmoothed.front();

  for(j=0; j<num; j++) {
    unsmoothed[j] = freq2pitch(rate() / unsmoothed[j]);
  }

  pitchBigSmoothingFilter->filter(unsmoothed.begin(), detailedPitchDataSmoothed.begin(), num);
  for(j=0; j<num; j++) detailedPitchDataSmoothed[j] = bound(detailedPitchDataSmoothed[j], 0.0f, topPitch);

  pitchSmallSmoothingFilter->filter(unsmoothed.begin(), detailedPitchData.begin(), num);
  for(j=0; j<num; j++) detailedPitchData[j] = bound(detailedPitchData[j], 0.0f, topPitch);
  return periodDiff;
}

void ChannelBase::processNoteDecisions(int chunk, float periodDiff, int analysisType, double topPitch, double noteThreshold)
{
  myassert(locked());
  myassert(dataAtChunk(chunk));
  AnalysisData &analysisData = *dataAtChunk(chunk);

  analysisData.reason = 0;
  //look for note transitions
  if(noteIsPlaying) {
    if(isVisibleChunk(&analysisData, noteThreshold) && !isNoteChanging(chunk)) {
    } else {
      noteIsPlaying = false;
      noteEnding(chunk, noteThreshold, topPitch);
    }
  } else { //if(!noteIsPlaying)
    if(isVisibleChunk(&analysisData, noteThreshold) /*&& !isChangingChunk(&analysisData)*/) {
      noteBeginning(chunk);
      periodDiff = 0.0f;
      noteIsPlaying = true;
    }
  }

  analysisData.notePlaying = noteIsPlaying;

  if(noteIsPlaying) {
      addToNSDFAggregate(dB2Linear(analysisData.logrms()), periodDiff);
      NoteData *currentNote = getLastNote();
      myassert(currentNote);

      analysisData.noteIndex = getCurrentNoteIndex();
      currentNote->setEndChunk(chunk+1);

      currentNote->addData(&analysisData, float(framesPerChunk()) / float(analysisData.period), topPitch);
      currentNote->setPeriodOctaveEstimate(calcOctaveEstimate());
      if(analysisType != MPM_MODIFIED_CEPSTRUM) {
        recalcNotePitches(chunk, analysisType, topPitch);
      }
  } else { //if(!noteIsPlaying)
  }
}

bool ChannelBase::isVisibleChunk(AnalysisData *data, double noteThreshold)
{
  myassert(data);
  if(data->noteScore() >= noteThreshold) return true;
  return false;
}

/** If the current note has shifted far enough away from the mean of the current Note
    then the note is changing.
*/
bool ChannelBase::isNoteChanging(int chunk)
{
  AnalysisData *prevData = dataAtChunk(chunk-1);
  if(!prevData) return false;
  myassert(dataAtChunk(chunk));
  myassert(noteData.size() > 0);
  AnalysisData *analysisData = dataAtChunk(chunk);
  //Note: analysisData.noteIndex is still undefined here
  int numChunks = getLastNote()->numChunks();

  float diff = fabs(analysisData->pitch - analysisData->shortTermMean);
  double spread = fabs(analysisData->shortTermMean - analysisData->longTermMean) -
    (analysisData->shortTermDeviation + analysisData->longTermDeviation);
  if(numChunks >= 5 && spread > 0.0) {
    analysisData->reason = 1;
    return true;
  }

  int firstShortChunk = MAX(chunk - (int)ceil(shortTime/timePerChunk()), getLastNote()->startChunk());
  AnalysisData *firstShortData = dataAtChunk(firstShortChunk);
  double spread2 = fabs(analysisData->shortTermMean - firstShortData->longTermMean) -
    (analysisData->shortTermDeviation + firstShortData->longTermDeviation);
  analysisData->spread = spread;
  analysisData->spread2 = spread2;

  if(numChunks >= (int)(ceil(longTime/timePerChunk()) / 2.0) && spread2 > 0.0) {
    analysisData->reason = 4;
    return true;
  }
  if(numChunks > 1 && diff > 2/*1.5*/) { //if jumping to fast anywhere then note is changing
    analysisData->reason = 2;
    return true;
  }
  return false;
}

void ChannelBase::noteEnding(int chunk, double noteThreshold, double topPitch)
{
  AnalysisData &analysisData = *dataAtChunk(chunk);
  if(analysisData.reason > 0) {
    backTrackNoteChange(chunk, noteThreshold, topPitch);
  }
}

void ChannelBase::noteBeginning(int chunk)
{
  AnalysisData *analysisData = dataAtChunk(chunk);
  noteData.push_back(NoteData(this, chunk, analysisData));
  resetNSDFAggregate(analysisData->period);

  pitchSmallSmoothingFilter->reset();
  pitchBigSmoothingFilter->reset();
}

void ChannelBase::addToNSDFAggregate(const float scaler, float periodDiff)
{
  AnalysisData &analysisData = *dataAtCurrentChunk();

  nsdfAggregateRoof += scaler;
  addElements(nsdfAggregateData.begin(), nsdfAggregateData.end(), nsdfData.begin(), scaler);

  NoteData *currentNote = getLastNote();
  myassert(currentNote);
  currentNote->nsdfAggregateRoof += scaler;
  currentNote->currentNsdfPeriod += periodDiff;
  float periodRatio = currentNote->currentNsdfPeriod / currentNote->firstNsdfPeriod;
  analysisData.periodRatio = periodRatio;
  int len = nsdfData.size();
  float stretch_len = float(len) * periodRatio;
  Array<float> stretch_data;
  stretch_data.resize(len);

  //the scaled version
  stretch_array(len, nsdfData.begin(), len, stretch_data.begin(), 0.0f, stretch_len, LINEAR);
  addElements(nsdfAggregateDataScaled.begin(), nsdfAggregateDataScaled.end(), stretch_data.begin(), scaler);
  copyElementsDivide(nsdfAggregateDataScaled.begin(), nsdfAggregateDataScaled.end(), currentNote->nsdfAggregateDataScaled.begin(), currentNote->nsdfAggregateRoof);

  //the unscaled version
  copyElementsDivide(nsdfAggregateData.begin(), nsdfAggregateData.end(), currentNote->nsdfAggregateData.begin(), currentNote->nsdfAggregateRoof);
}

NoteData *ChannelBase::getLastNote()
{
  return (noteData.empty()) ? NULL : &noteData.back();
}

/** Uses the nsdfAggregateData to get an estimate of
    the period, over a whole note so far.
    This can be used to help determine the correct octave throughout the note
    @return the estimated note period, or -1.0 if none found
*/
float ChannelBase::calcOctaveEstimate() {
  Array<float> agData = nsdfAggregateDataScaled;
  std::vector<int> nsdfAggregateMaxPositions;
  MyTransforms::findNSDFMaxima(agData.begin(), agData.size(), nsdfAggregateMaxPositions);
  if(nsdfAggregateMaxPositions.empty()) return -1.0;

  //get the highest nsdfAggregateMaxPosition
  uint j;
  int nsdfAggregateMaxIndex = 0;
  for(j=1; j<nsdfAggregateMaxPositions.size(); j++) {
    if(agData[nsdfAggregateMaxPositions[j]] > agData[nsdfAggregateMaxPositions[nsdfAggregateMaxIndex]]) nsdfAggregateMaxIndex = j;
  }
  //get the choosen nsdfAggregateMaxPosition
  double highest = agData[nsdfAggregateMaxPositions[nsdfAggregateMaxIndex]];
  double nsdfAggregateCutoff = highest * threshold();
  /*Allow for overide by the threshold value*/
  int nsdfAggregateChoosenIndex = 0;
  for(j=0; j<nsdfAggregateMaxPositions.size(); j++) {
    if(agData[nsdfAggregateMaxPositions[j]] >= nsdfAggregateCutoff) { nsdfAggregateChoosenIndex = j; break; }
  }
  float periodEstimate = float(nsdfAggregateMaxPositions[nsdfAggregateChoosenIndex]+1); //add 1 for index offset, ie position 0 = 1 period
  return periodEstimate;
}

void ChannelBase::recalcNotePitches(int chunk, int analysisType, double topPitch)
{
  if(!isValidChunk(chunk)) return;
  //recalculate the values for the note using the overall periodOctaveEstimate
  NoteData *currentNote = getLastNote();
  if(currentNote == NULL) return;
  myassert(currentNote);
  int first = currentNote->startChunk();
  int last = chunk; //currentNote->endChunk();
  currentNote->resetData();
  int numNotesChangedIndex = 0;
  for(int curChunk=first; curChunk<=last; curChunk++) {
    if(chooseCorrelationIndex(curChunk, periodOctaveEstimate(curChunk), analysisType,
                              topPitch)) numNotesChangedIndex++;
    calcDeviation(curChunk);
    currentNote->addData(dataAtChunk(curChunk), float(framesPerChunk()) / float(dataAtChunk(curChunk)->period), topPitch);
  }
}

void ChannelBase::backTrackNoteChange(int chunk, double noteThreshold, double topPitch) {
  int first = MAX(chunk - (int)ceil(longTime/timePerChunk()), getLastNote()->startChunk()/*+5*/);
  int last = chunk; //currentNote->endChunk();
  if(first >= last) return;
  float largestWeightedDiff = 0.0f; //fabs(dataAtChunk(first)->pitch - dataAtChunk(first)->shortTermMean);
  int largestDiffChunk = first;
  for(int curChunk=first+1, j=1; curChunk<=last; curChunk++, j++) {
    float weightedDiff = fabs(dataAtChunk(curChunk)->pitch - dataAtChunk(curChunk)->shortTermMean) /** float(maxJ-j)*/;
    if(weightedDiff > largestWeightedDiff) {
      largestWeightedDiff = weightedDiff;
      largestDiffChunk = curChunk;
    }
  }
  getLastNote()->setEndChunk(largestDiffChunk);
  getLastNote()->recalcAvgPitch(topPitch);
  dataAtChunk(largestDiffChunk)->reason = 5;

  //start on next note
  for(int curChunk = largestDiffChunk; curChunk <= last; curChunk++) {
    dataAtChunk(curChunk)->noteIndex = NO_NOTE;
    dataAtChunk(curChunk)->notePlaying = false;
    dataAtChunk(curChunk)->shortTermMean = dataAtChunk(curChunk)->pitch;
    dataAtChunk(curChunk)->longTermMean = dataAtChunk(curChunk)->pitch;
    dataAtChunk(curChunk)->shortTermDeviation = 0.2f;
    dataAtChunk(curChunk)->longTermDeviation = 0.05f;
    dataAtChunk(curChunk)->periodRatio = 1.0f;
  }

  int curChunk = largestDiffChunk;
  if(curChunk < last) {
    curChunk++;
  }
  while((curChunk < last) && !isVisibleChunk(dataAtChunk(curChunk), noteThreshold)) {
    curChunk++;
  }
  if((curChunk < last) && isVisibleChunk(dataAtChunk(curChunk), noteThreshold)) {
    noteIsPlaying = true;
    noteBeginning(curChunk);
    NoteData *currentNote = getLastNote();
    myassert(currentNote);
    dataAtChunk(curChunk)->noteIndex = getCurrentNoteIndex();
    dataAtChunk(curChunk)->notePlaying = true;
    curChunk++;
    while((curChunk < last) && isVisibleChunk(dataAtChunk(curChunk), noteThreshold)) {
      dataAtChunk(curChunk)->noteIndex = getCurrentNoteIndex();
      dataAtChunk(curChunk)->notePlaying = true;
      currentNote->addData(dataAtChunk(curChunk), float(framesPerChunk()) / float(dataAtChunk(curChunk)->period), topPitch);
      curChunk++;
    }
    resetNSDFAggregate(dataAtChunk(last-1)->period); //just start the NSDF Aggregate from where we are now
    //just start with the octave estimate from the last note
    currentNote->setPeriodOctaveEstimate(getNote(getCurrentNoteIndex()-1)->periodOctaveEstimate());
  }
}

void ChannelBase::resetNSDFAggregate(float period)
{
  nsdfAggregateRoof = 0.0;
  std::fill(nsdfAggregateData.begin(), nsdfAggregateData.end(), 0.0f);
  std::fill(nsdfAggregateDataScaled.begin(), nsdfAggregateDataScaled.end(), 0.0f);

  NoteData *currentNote = getLastNote();
  myassert(currentNote);
  currentNote->nsdfAggregateRoof = 0.0;
  currentNote->currentNsdfPeriod = currentNote->firstNsdfPeriod = period;
}

NoteData *ChannelBase::getNote(int noteIndex)
{
  if(noteIndex >= 0 && noteIndex < (int)noteData.size()) return &noteData[noteIndex];
  else return NULL;
}

void ChannelBase::calcVibratoData(int chunk)
{
  NoteData *currentNote = getLastNote();
  if (currentNote && (dataAtChunk(chunk)->noteIndex >=0)) {
    currentNote->addVibratoData(chunk);
  }
}
