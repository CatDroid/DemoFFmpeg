//
// Created by hanlon on 17-1-5.
//

#include <inttypes.h>
#include "BufferManager.h"
#include "Buffer.h"


CLASS_LOG_IMPLEMENT(BufferManager,"BufferManager");

//#define TRACE_BUFFER_MANAGER 1

BufferManager::BufferManager(const char* const who, uint32_t perBufSize , uint32_t maxBufs)
        :mNativeSize(0),mPerBufSize(perBufSize),mMaxFreeBufs(maxBufs) {
    size_t wholen = strlen(who);
    if(wholen > 0){
        memset(mWho,0,sizeof(mWho));
        strncpy(mWho, who, wholen <= sizeof(mWho) - 1 ? wholen : sizeof(mWho) - 1 );
    }else{
        strcpy( mWho, "unknown");
    }

    mBufMutex = new Mutex();
    TLOGW("BufferManager max %u per %u for %s" ,mMaxFreeBufs , mPerBufSize , mWho  );
}

sp<Buffer> BufferManager::pop()
{
    AutoMutex temp(mBufMutex);

    if( mFreeBuffers.empty() ){
        Buffer* pbuf = new Buffer(this);
        bool done = pbuf->alloc(mPerBufSize);
        if(!done){
            TLOGE("av_malloc fail at get " );
            return NULL;
        }
        mNativeSize += mPerBufSize;
        mTotalBuffers.push_back(pbuf);
#if TRACE_BUFFER_MANAGER == 1
        TLOGT("%s malloc one buffer %p , mNativeSize %" PRId64 " free %lu total %lu",
              mWho, pbuf, mNativeSize , mFreeBuffers.size(),mTotalBuffers.size() );
#endif
        return pbuf ;
    }else{
        Buffer* pbuf =mFreeBuffers.front();
        mFreeBuffers.pop_front();
        pbuf->reset(this);
#if TRACE_BUFFER_MANAGER == 1
        TLOGT("%s get from free list %p free %lu total %lu " ,
               mWho, pbuf, mFreeBuffers.size() ,mTotalBuffers.size() );
#endif
        return pbuf;
    }
}

void BufferManager::push(Buffer *pbuf)
{
    AutoMutex l(mBufMutex);
    if(mFreeBuffers.size() >= mMaxFreeBufs ){
        // 如果空余的Buffer超过mMaxBufs 后面返回的Buffer将会释放
        // 但是如果空余的Buffer少于mMaxBufs,返回的Buffer放到mFreeBuffers
        // Buffer最大数目由外部控制,所以mTotalBuffer.size()可能会一直增大,如果外部获取Buffer的速率一直大于释放Buffer的速率
        mTotalBuffers.remove(pbuf);
        mNativeSize-= mPerBufSize ;
#if TRACE_BUFFER_MANAGER == 1
        TLOGT("%s free one buffer %p [FULL] mNativeSize %" PRId64 " free %lu total %lu"  ,
              mWho, pbuf , mNativeSize,mFreeBuffers.size(),mTotalBuffers.size() );
#endif
        delete pbuf;
    }else{
        mFreeBuffers.push_back(pbuf);
        pbuf->mBM = NULL;
#if TRACE_BUFFER_MANAGER == 1
        TLOGT("%s put to free list %p free %lu total %lu" ,
              mWho, pbuf , mFreeBuffers.size() ,mTotalBuffers.size() );
#endif
    }
#if TRACE_BUFFER_MANAGER == 1
    TLOGT("%s after push ref_count %d " , mWho, this->ref_count() );
#endif
}

BufferManager::~BufferManager()
{
    TLOGW("~BufferManager entry" );
    for ( std::list<Buffer*>::iterator it = mTotalBuffers.begin();
         it != mTotalBuffers.end(); it++) {
        Buffer *pbuf = *it;
        delete pbuf;
#if TRACE_BUFFER_MANAGER == 1
        TLOGT("%s free one buffer %p" , mWho , pbuf);
#endif
    }
    delete mBufMutex ; mBufMutex = NULL ;
    TLOGW("~BufferManager done" );
}
