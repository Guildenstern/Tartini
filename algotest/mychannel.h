#ifndef MYCHANNEL_H
#define MYCHANNEL_H

#include "channelbase.h"
#include "analysisdata.h"
#include "notedata.h"

class MyChannel : public ChannelBase {
public:
    MyChannel(double timeperchunk_, int sampleRate_, int framesperchunk,
        int size, int k);
    bool doingDetailedPitch();
    double dBFloor();
    bool firstTimeThrough();
    int currentChunk();
};

#endif
