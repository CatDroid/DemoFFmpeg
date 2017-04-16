//
// Created by hanlon on 17-1-16.
//

#include "MyPacket.h"

#define LOG_TAG "PacketManager"
#include "jni_common.h"

PacketManager::PacketManager(uint32_t maxPacketsNum  )
        :mMaxPackets(maxPacketsNum)  {
    mpMutex = new Mutex();
    ALOGD("%p constructor mMaxPackets %d " , this, mMaxPackets );
}

sp<MyPacket> PacketManager::pop()
{
    AutoMutex temp(mpMutex);

    if( mFreePackets.empty() ){
        // try {
        MyPacket* pbuf = new MyPacket(this);
        // catch (bad_alloc new_error){
        //
        //}
        ALOGD("%p new one packet %p" , this , pbuf );
        mTotalPackets.push_back(pbuf);
        return pbuf ;
    }else{
        MyPacket* pbuf =mFreePackets.front();
        mFreePackets.pop_front();
        pbuf->reset(this);
        return pbuf;
    }
}

void PacketManager::push(MyPacket *pbuf)
{
    AutoMutex l(mpMutex);
    //ALOGD("mFreePackets %d mTotalPackets %d mMaxPackets %d ref_count %d " ,
    //      mFreePackets.size() ,mTotalPackets.size(), mMaxPackets , this->ref_count() );
    if(mFreePackets.size() >= mMaxPackets ){
        mTotalPackets.remove(pbuf);
        ALOGD("%p free one packet %p [FULL]" , this , pbuf );
        delete pbuf; // 导致析构  MyPacket.mPm 在这里 = NULL 所以不能返回后调用 mPm = NULL ;
    }else{
        ALOGD("%p put to freePackets %p [before size %lu] " ,this , pbuf , mFreePackets.size() );
        mFreePackets.push_back(pbuf);
        pbuf->mPm = NULL ; //  已经把MyPacket还给 PacketManager  取消对PacketManager的引用
    }
}

PacketManager::~PacketManager()
{
    ALOGD("%p deconstructor " , this );
    for ( std::list<MyPacket*>::iterator it = mTotalPackets.begin();
          it != mTotalPackets.end(); it++)
    {
        MyPacket *pbuf = *it;
        ALOGD("%p deconstructor free one packet %p" , this ,pbuf );
        delete pbuf;
    }
    ALOGD("%p deconstructor done" , this );
    delete mpMutex ; mpMutex = NULL ;
}




MyPacket::MyPacket( sp<PacketManager> bm )
        :mPts(0),mPm(bm)
{
    mpPacket = av_packet_alloc();
    av_init_packet(mpPacket);
}

MyPacket::~MyPacket() // 只有PacketManger调用
{
    av_packet_free(&mpPacket);
    mpPacket = NULL;
}

void MyPacket::reset(sp<PacketManager> pm)
{
    av_init_packet(mpPacket);
    mPts = 0 ;
    mPm = pm ;
}

void MyPacket::recycle(){
    av_packet_unref(mpPacket);// 这里会把AVPacket清空 没有AVBufferRef*了
    mPm->push(this);
}


