//
// Created by hanlon on 17-1-16.
//

#ifndef DEMO_FFMPEG_AS_MYPACKET_H
#define DEMO_FFMPEG_AS_MYPACKET_H


extern "C"{
#include "libavcodec/avcodec.h"
};

#include <list>
#include "Buffer.h"

class MyPacket;
class PacketManager : public LightRefBase<PacketManager> {

public:
    PacketManager( uint32_t maxPacketsNum);
    sp<MyPacket> pop();
    void push(MyPacket *buf);
    ~PacketManager();
private:
    uint32_t mMaxPackets ;
    Mutex* mpMutex ;
    std::list<MyPacket*> mTotalPackets;
    std::list<MyPacket*> mFreePackets;
};

class MyPacket : public RecycleRefBase<MyPacket> {
public:
    MyPacket(sp<PacketManager> pm);
    virtual ~MyPacket();
    int64_t& pts() {return mPts;}
    AVPacket* packet() {return mpPacket; }
    void reset(sp<PacketManager> pm);

private:
    friend class PacketManager ;
    AVPacket* mpPacket;
    int64_t mPts ;
    void recycle();
    sp<PacketManager> mPm;
};


#endif //DEMO_FFMPEG_AS_MYPACKET_H
