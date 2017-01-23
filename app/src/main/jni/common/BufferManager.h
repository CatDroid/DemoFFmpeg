//
// Created by hanlon on 17-1-5.
//

#ifndef DEMO_FFMPEG_AS_BUFFERMANAGER_H
#define DEMO_FFMPEG_AS_BUFFERMANAGER_H

#include <list>
#include "LightRefBase.h"
#include "Mutex.h"


class Buffer ;
class BufferManager : public LightRefBase<BufferManager> {

public:
    BufferManager(uint32_t perBufSize , uint32_t maxBufs);
    ~BufferManager();
    sp<Buffer> pop();
    void push(Buffer *buf);

private:
    uint32_t mPerBufSize ;
    uint32_t mMaxBufs ;
    Mutex* mBufMutex ;
    std::list<Buffer*> mTotalBuffers;
    std::list<Buffer*> mFreeBuffers;
};


#endif //DEMO_FFMPEG_AS_BUFFERMANAGER_H
