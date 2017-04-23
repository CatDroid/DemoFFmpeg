//
// Created by hanlon on 17-1-5.
//

#ifndef DEMO_FFMPEG_AS_BUFFERMANAGER_H
#define DEMO_FFMPEG_AS_BUFFERMANAGER_H

#include <list>
#include "LightRefBase.h"
#include "Mutex.h"
#include "Log.h"


class Buffer ;
class BufferManager : public LightRefBase<BufferManager> {

public:
    BufferManager(const char* const who, uint32_t perBufSize , uint32_t maxBufs);
    ~BufferManager();
    sp<Buffer> pop();
    void push(Buffer *buf);

private:
    uint64_t mNativeSize ;
    uint32_t mPerBufSize ;
    uint32_t mMaxFreeBufs ;  // 空余的Buffer最大数目,并没有控制最大的Buffer数目(由外部控制)
    Mutex* mBufMutex ;
    std::list<Buffer*> mTotalBuffers;
    std::list<Buffer*> mFreeBuffers;
    char mWho[16] ;

private:
    CLASS_LOG_DECLARE(BufferManager);
};


#endif //DEMO_FFMPEG_AS_BUFFERMANAGER_H
