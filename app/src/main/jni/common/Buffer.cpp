//
// Created by hanlon on 17-1-5.
//

#include "Buffer.h"
#include "BufferManager.h"
extern "C"{
#include "libavutil/mem.h"
}


Buffer::Buffer( sp<BufferManager>bm )
        :mPts(0),mPtr(NULL),mSize(0),mTotalPtr(NULL),mTotalSize(0),mBM(bm)
{
}

Buffer::~Buffer()
{
    if( mTotalPtr != NULL){
        av_freep(&mTotalPtr);
        mTotalPtr = NULL;
        mTotalSize = 0 ;
    }
}

bool Buffer::alloc(int32_t size)
{
    mPtr = mTotalPtr = (uint8_t*)av_malloc(size); // malloc 可能不对齐
    if( mTotalPtr == NULL ){
        return false ;
    }else{
        mSize = mTotalSize = size ;
        return true ;
    }
}

void Buffer::reset(sp<BufferManager>bm)
{
    mPtr = mTotalPtr;
    mSize = mTotalSize;
    mPts = 0 ;
    mBM = bm ;
}


void Buffer::recycle(){
    mBM->push(this);
    mBM = NULL;
}
