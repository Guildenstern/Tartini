#include "mychannel.h"

MyChannel::MyChannel(double timeperchunk, int sampleRate, int framesperchunk,
                     int size, int k) 
       :ChannelBase(timeperchunk, sampleRate, framesperchunk, size, k) {
}

bool
MyChannel::doingDetailedPitch() {
    return true;
}
double
MyChannel::dBFloor() {
    return 0;
}
bool
MyChannel::firstTimeThrough() {
    return true;
}
int
MyChannel::currentChunk() {
    return 0;
}
