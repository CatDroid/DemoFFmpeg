//
// Created by hanlon on 17-1-5.
//

#include "BufferManager.h"
#include "Buffer.h"

#define LOG_TAG "BufferManager"
#include "jni_common.h"

BufferManager::BufferManager(uint32_t perBufSize , uint32_t maxBufs)
        :mPerBufSize(perBufSize),mMaxBufs(maxBufs) {

    ALOGD("%p constructor " , this );
}

sp<Buffer> BufferManager::pop()
{
    AutoMutex temp(mBufMutex);

    if( mFreeBuffers.empty() ){
        Buffer* pbuf = new Buffer(this);
        bool done = pbuf->alloc(mPerBufSize);
        if(!done){
            ALOGE("%p av_malloc fail at get ", this );
            return NULL;
        }
        ALOGD("%p malloc one buffer %p" , this , pbuf );
        mTotalBuffers.push_back(pbuf);
        return pbuf ;
    }else{
        Buffer* pbuf =mFreeBuffers.front();
        mFreeBuffers.pop_front();
        pbuf->reset(this);
        return pbuf;
    }
}

void BufferManager::push(Buffer *pbuf)
{
    AutoMutex l(mBufMutex);
    if(mFreeBuffers.size() >= mMaxBufs ){
        mTotalBuffers.remove(pbuf);
        ALOGD("%p free one buffer %p [FULL]" , this , pbuf );
        delete pbuf;
    }else{
        ALOGD("%p put to freeBuffer %p" ,this , pbuf);
        mFreeBuffers.push_back(pbuf);
    }
}

BufferManager::~BufferManager()
{
    ALOGD("%p deconstructor " , this );
    for ( std::list<Buffer*>::iterator it = mTotalBuffers.begin();
         it != mTotalBuffers.end(); it++)
    {
        Buffer *pbuf = *it;
        ALOGD("%p free one buffer %p" , this ,pbuf );
        delete pbuf;
    }
}
