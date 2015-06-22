#ifndef CHANNELBASE_H
#define CHANNELBASE_H

#include "array1d.h"
#include <deque>

class Filter;
class AnalysisData;
class NoteData;

class ChannelBase {
protected:
    const static double longTime;
    const static double shortTime;
    float _threshold;
    std::deque<AnalysisData> lookup;
    double timeperchunk;
    int sampleRate;
    bool noteIsPlaying;
    int pronyWindowSize;
    Array<float> pronyData;
    int framesperchunk;
    double nsdfAggregateRoof; //keeps the sum of scalers. i.e. The highest possible aggregate value
public:
    std::deque<NoteData> noteData;
    Filter *pitchBigSmoothingFilter;
    Filter *pitchSmallSmoothingFilter;

    ChannelBase(double timeperchunk, int sampleRate, int framesperchunk, int size, int k);
    virtual ~ChannelBase();
    int totalChunks() { return lookup.size(); }
    int size() { return directInput.size(); }
    bool isValidChunk(int chunk) { return (chunk >= 0 && chunk < totalChunks()); }
    AnalysisData *dataAtChunk(int chunk) { return (isValidChunk(chunk)) ? &lookup[chunk] : NULL; }
    float threshold() { return _threshold; }
    void chooseCorrelationIndex1(int chunk, double topPitch);
    bool chooseCorrelationIndex(int chunk, float periodOctaveEstimate,
                                int analysisType, double topPitch);
    bool isFirstChunkInNote(int chunk);
    bool isNotePlaying() const { return noteIsPlaying; }
    float periodOctaveEstimate(int chunk); /*< A estimate from over the whole duration of the note, to help get the correct octave */
    void calcDeviation(int chunk);
    int getCurrentNoteIndex() { return int(noteData.size())-1; }
    double timePerChunk() const { return timeperchunk; }
    void doPronyFit(int chunk);
    int pronyDelay() { return pronyWindowSize/2; }
    int rate() { return sampleRate; }
    float calcDetailedPitch(float *input, double period, int chunk, float topPitch);
    void processNoteDecisions(int chunk, float periodDiff, int analysisType,
                              double topPitch, double noteThreshold);
    bool isNoteChanging(int chunk);
    bool isVisibleChunk(AnalysisData *data, double noteThreshold);
    bool isVisibleChunk(int chunk_, double noteThreshold) { return isVisibleChunk(dataAtChunk(chunk_), noteThreshold); }
    void noteEnding(int chunk, double noteThreshold, double topPitch);
    void noteBeginning(int chunk);
    void addToNSDFAggregate(const float scaler, float periodDiff);
    NoteData *getLastNote();
    int framesPerChunk() { return framesperchunk; }
    float calcOctaveEstimate();
    void recalcNotePitches(int chunk, int analysisType, double topPitch);
    void backTrackNoteChange(int chunk, double noteThreshold, double topPitch);
    void resetNSDFAggregate(float period);
    AnalysisData *dataAtCurrentChunk() { return dataAtChunk(currentChunk()); }
    NoteData *getNote(int noteIndex);

    void calcVibratoData(int chunk);
    virtual bool doingDetailedPitch() = 0;
    virtual double dBFloor() = 0;
    virtual bool firstTimeThrough() = 0;
    virtual int currentChunk() = 0; //this one should be use to retrieve current info

    Array<float> directInput;
    Array<float> filteredInput;
    Array<float> nsdfData;
    double rmsFloor; //in dB
    double rmsCeiling; //in dB
    Array<float> fftData1;
    Array<float> fftData2;
    std::deque<float> pitchLookup;
    std::deque<float> pitchLookupSmoothed;
    Array<float> detailedPitchData;
    Array<float> detailedPitchDataSmoothed;
    Array<float> cepstrumData;

private:
    Array<float> nsdfAggregateData;
    Array<float> nsdfAggregateDataScaled;
};

#endif // CHANNELBASE_H
