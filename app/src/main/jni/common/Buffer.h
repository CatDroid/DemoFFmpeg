//
// Created by hanlon on 17-1-5.
//

#ifndef DEMO_FFMPEG_AS_BUFFER_H
#define DEMO_FFMPEG_AS_BUFFER_H


#include "LightRefBase.h"
#include "ScopedX.h"
#include "Mutex.h"

class BufferManager;

#define COMPARE(_op_)                                         \
inline bool operator _op_ (const Buffer o) const {            \
    return mPts _op_ o.mPts;                                  \
}                                                             \
inline bool operator _op_ (const Buffer* const o) const {     \
    return mPts _op_ o->mPts;                                 \
}

class Buffer : public RecycleRefBase<Buffer>{

public:
    Buffer(sp<BufferManager>bm);
    bool alloc(int32_t size);
    void reset(sp<BufferManager>bm);
    uint8_t* data(){return mPtr;}
    int64_t& pts() {return mPts;} //us
    int32_t& size() {return mSize;}
    int32_t& width() {return mWidth;}
    int32_t& height() {return mHeight;}
    int32_t& fmt() {return mFmt;}
    virtual ~Buffer();

    COMPARE(>)
    COMPARE(<)
    COMPARE(>=)
    COMPARE(<=)
    COMPARE(==)

private:
    friend class BufferManager ;
    int64_t mPts ;
    int32_t mWidth;
    int32_t mHeight;
    int32_t mFmt ; // AVPixelFormat
    uint8_t* mPtr ;
    int32_t mSize ;
    uint8_t* mTotalPtr ;
    int32_t mTotalSize ;
    sp<BufferManager> mBM ;
    void recycle();
};

#undef COMPARE


//__attribute__((deprecated))
//class ScopedBuffer : public ScopedX<Buffer>{
//
//protected:
//    virtual void release(){
//        //p->unref() ;
//    }
//    virtual void obtain(){
//        //p->addref();
//    }
//};





#endif //DEMO_FFMPEG_AS_BUFFER_H
