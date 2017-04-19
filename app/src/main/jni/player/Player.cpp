//
// Created by Hanloong Ho on 17-4-6.
//


#include <pthread.h>
#include "Player.h"

#define LOG_TAG "Player"
#include "jni_common.h"

#include "DeMuxer.h"
#include "Decoder.h"
#include "LocalFileDemuxer.h"
#include "DecoderFactory.h"
#include "JClass.h"
#include "JEvent.h"
#include "RenderThread.h"

CLASS_LOG_IMPLEMENT(Player,"Player");

extern JNIDragonPlayer gJNIDragonPlayer ;

Player::Player(JNIEnv* env , jobject jWeakRef):mjObjWeakRef(NULL),mWindow(NULL),
                 mDeMuxer(NULL),mVDecoder(NULL),mADecoder(NULL),mRender(NULL),
        mState(STATE::UNINIT),mStop(false),
        mCmdMutex(NULL),mCmdCond(NULL),
        mSeekToMs(-1){
    mjObjWeakRef = env->NewGlobalRef(jWeakRef);
    mCmdMutex = new Mutex();
    mCmdCond = new Condition();
    int ret = pthread_create(&mThreadID, NULL, sStateMachineTh , this);
    if(ret != 0 ){
        TLOGE("Player pthread_create fail with %d %d %s", ret , errno , strerror(errno));
    }
    assert(ret == 0);
}


bool Player::setDataSource(std::string uri)
{
    if( mState == UNINIT || mState == INITED){ // 允许重复设置路径
        mPlayURI = uri ;
        setState(INITED);
    }else{
        return false ;
    }
    return true ;
}

bool Player::setView(JNIEnv* env , jobject surface) {
    if( mState == UNINIT || mState == INITED){
        if(mWindow != NULL){
            TLOGW("duplicate setView, release the older one %p",  mWindow);
            ANativeWindow_release(mWindow);
        }
        mWindow = ANativeWindow_fromSurface(env,surface);
        return true ;
    }
    TLOGE("setView state %s ", stateId2Str(mState));
    return false ;
}


bool Player::prepare() {
    if(mState == INITED ){
        setState(PREPARING);
        sendEvent(EVENT::CMD_PREPARE);
    }else{
        TLOGE("prepare state %s ", stateId2Str(mState));
        return false;
    }
    return true ;
}

void Player::prepareSync(){
    TLOGE("NO IMPLEMENT");
    assert(false );
}

bool Player::play() {
    if(mState == PREPARED || mState == PAUSED  || mState == PLAYING){
        sendEvent(EVENT::CMD_PLAY);
    }else{
        TLOGE("play state %s ", stateId2Str(mState));
        return false ;
    }
    return true ;
}

bool Player::pause() {
    if(mState == PLAYING || mState == PAUSED){
        sendEvent(EVENT::CMD_PAUSE);
        return true ;
    }
    TLOGE("pause state %s ", stateId2Str(mState));
    return false ;
}

bool Player::seekTo(int32_t ms) {
    if(mState == PLAYING || mState == PAUSED || mSeekToMs != -1 /*last seek is not complete*/){
        mSeekToMs = ms ;
        sendEvent(CMD_SEEK);
    }else{
        TLOGE("seekTo state %s ", stateId2Str(mState));
        return false ;
    }
    return true ;
}

void Player::stop() {
    if(mState == PLAYING || mState == PAUSED){
        sendEvent(CMD_STOP);
    }else{
        TLOGE("stop state %s ", stateId2Str(mState));
    }
}

// 内部接口
void Player::prepare_result() { // 来自Demuxer
    sendEvent( EVENT::CMD_PREPARE_RESULT );
}

Player::~Player()
{
    {
        AutoMutex _l(mCmdMutex);
        mCmdCond->signal();
    }

    TLOGD("Player pthread_join enter ");
    pthread_join(mThreadID, NULL);
    TLOGD("Player pthread_join join");


    delete mCmdMutex; mCmdMutex = NULL ;
    delete mCmdCond ; mCmdCond = NULL;
    TLOGD("~Player");
}

void* Player::sStateMachineTh(void* ctx )
{
    JNIEnv* env ;
    LOGW(s_logger, "sStateMachineTh Entry ");
    JNI_ATTACH_JVM_WITH_NAME(env,"Player");
    ((Player*)ctx)->loop(env) ;
    JNI_DETACH_JVM(env);
    LOGW(s_logger, "sStateMachineTh Exit ");
    return NULL;
}

void Player::loop(void* ctx  )
{
    JNIEnv* jenv = (JNIEnv*)ctx;

    bool pass = false ;

    TLOGD("loop enter");
    while(!mStop) {

        EVENT cmd = EVENT::CMD_UNDEFINE ;
        {
            AutoMutex _l(mCmdMutex);
            if( mCmdQueue.empty() ){
                TLOGD("Wait CMD");
                mCmdCond->wait(mCmdMutex);
                continue ;
            }else{
                cmd = mCmdQueue.front();
                mCmdQueue.pop_front();
            }
        }

        TLOGD("Excute CMD %d STATE %s ", cmd , stateId2Str(mState));
        if( cmd == EVENT::CMD_UNDEFINE) continue ;
        pass = false ;

        switch(mState){
            case STATE::UNINIT:{
            }break;
            case STATE::INITED:{
            }break;
            case STATE::PREPARING:{
                if(cmd == EVENT::CMD_PREPARE) {
                    TLOGW("prepare %s ", mPlayURI.c_str());
                    std::size_t pos = mPlayURI.find("://");
                    if (pos == std::string::npos || mPlayURI.size() <= 7) {
                        TLOGE("CMD_PREPARE in PREPARING parse uri error uri:%s size %lu ",
                              mPlayURI.c_str(),  mPlayURI.size());
                        setState(STATE::ERROR);
                        notify(jenv,MEDIA_ERR_PREPARE);
                        continue;
                    }
                    if( mPlayURI.compare(0,4,"file") == 0 ){
                        mDeMuxer = new LocalFileDemuxer(this);
                        mDeMuxer->setDataSource(mPlayURI);
                        mDeMuxer->prepareAsyn();
                    }else if( mPlayURI.compare(0,4,"rtsp") == 0 ){
                        TLOGE("NO IMPLEMENT");
                        assert(false);
                    }else if( mPlayURI.compare(0,4,"rtmp") == 0 ){
                        TLOGE("NO IMPLEMENT");
                        assert(false);
                    }
                }else if(cmd == EVENT::CMD_PREPARE_RESULT){
                    if( mDeMuxer->getParseResult() ){

                        if( mDeMuxer->getAudioCodecPara() != NULL){
                            TLOGW("source has Audio !");
                            mADecoder = DecoderFactory::create(mDeMuxer->getAudioCodecPara(),
                                                  mDeMuxer->getAudioTimeBase());
                            if(mADecoder == NULL){
                                // TODO
                                TLOGE("audio decoder create FAIL !");
                            }
                        }
                        if( mDeMuxer->getVideoCodecPara() != NULL && mWindow != NULL){// 如果没有设置Window不会创建视频解码器
                            TLOGW("source has Video !");
                            mVDecoder = DecoderFactory::create(mDeMuxer->getVideoCodecPara(),
                                                               mDeMuxer->getVideoTimeBase());
                            if(mVDecoder == NULL){
                                // TODO
                                TLOGE("video decoder create FAIL !");
                            }
                        }
                        setState(STATE::PREPARED);
                        notify(jenv,MEDIA_PREPARED);
                    }else{
                        setState(STATE::ERROR);
                        notify(jenv,MEDIA_ERR_PREPARE);
                    }
                }
            }break;
            case STATE::PREPARED:{
                if(cmd == EVENT::CMD_PLAY){

                    mRender = new RenderThread();
//#define TEST_AUDIO_ONLY

#ifndef TEST_VIDEO_ONLY
                    if(mADecoder != NULL){
                        mADecoder->setRender(mRender);
                        mADecoder->start();
                        mDeMuxer->setAudioDecoder(mADecoder);
                        mRender->setAudioParam(
                                mDeMuxer->getAudioCodecPara()->channels,
                                mDeMuxer->getAudioCodecPara()->sample_rate,
                                16 // 我们总是转换成16bit的 S16
                        );
                    }
#endif
#ifndef TEST_AUDIO_ONLY
                    if(mVDecoder != NULL){
                        mVDecoder->start();
                        mVDecoder->setRender(mRender);
                        mDeMuxer->setVideoDecoder(mVDecoder);
                        mRender->setVideoParam(
                                12,
                                mDeMuxer->getVideoCodecPara()->width,
                                mDeMuxer->getVideoCodecPara()->height);
                        mRender->setPreview(mWindow);

                    }
#endif
                    mRender->start();
                    mDeMuxer->play();

                    /*
                     * 避免 ffmpeg 内部创建的线程 也继承
                     * 避免 其他线程占用cpu导致 事件线程无法响应
                     */
                    //int n = nice(-10);
                    //TLOGW("change to nice %d " , n);

                    pthread_attr_t attr ; memset(&attr,0,sizeof(pthread_attr_t));
                    int policy = 0  ;
                    pthread_attr_getschedpolicy(&attr, &policy);
                    if( policy  == SCHED_OTHER){
                        TLOGW("sched %s priority %d ", "SCHED_OTHER" ,attr.__sched_priority );
                    }else if (policy == SCHED_RR ){
                        TLOGW("sched %s priority %d ", "SCHED_RR" ,attr.__sched_priority );
                    }else if (policy == SCHED_FIFO ){
                        TLOGW("sched %s priority %d ", "SCHED_FIFO" ,attr.__sched_priority );
                    }else {
                        TLOGW("sched %s %d priority %d ", "unkown" , policy , attr.__sched_priority );
                    }

                    setState(STATE::PLAYING);
                }

            }break;
            case STATE::PLAYING:{

                if(cmd == EVENT::CMD_PAUSE){
                    // TODO
                    setState(STATE::PAUSING);
                    pass = true ;
                }else if(cmd == EVENT::CMD_PLAY){
                    // Play And Play again
                    // TODO onNotify
                    pass = true ;
                }else if(cmd == EVENT::CMD_PLAY_COMPLETE){

                }else if(cmd == EVENT::CMD_STOP){
                    setState(STATE::STOPPING);
                    // 调用顺序 应该是 Render --> Decorder --> DeMuxer 因为后者可能挂在前者的缓冲队列上
                    if(mRender != NULL ){
                        TLOGW("call Render stop");
                        mRender->stop();
                    }
                    if(mADecoder != NULL){
                        TLOGW("call ADecorder stop");
                        mADecoder->stop();
                    }
                    if(mVDecoder != NULL){
                        TLOGW("call VDecorder stop");
                        mVDecoder->stop();
                    }
                    if(mDeMuxer != NULL){
                        TLOGW("call DeMuxer stop");
                        mDeMuxer->stop();
                    }

                    // TODO 支持stop之后重新play的话 这里只需要析构Decoder和Render 保留Demuxer
                    delete mRender  ;   mRender = NULL;
                    delete mADecoder;   mADecoder = NULL;
                    delete mVDecoder;   mVDecoder = NULL;
                    delete mDeMuxer ;   mDeMuxer = NULL;

                    setState(STATE::STOPPED);

                    mStop = true ; // 退出线程
                }
            }break;
            case STATE::PAUSING:{
                TLOGW("PAUSING status");
            }break;
            case STATE::PAUSED:{
                TLOGW("PAUSED status");
            }break;
            case STATE::STOPPING:{
                TLOGW("STOPPING status");
            }break;
            case STATE::STOPPED:{
                TLOGW("STOPPED status");
            }break;
            case STATE::SEEKING: {
                TLOGW("SEEKING status");
            }break;
            case STATE::ERROR:{
                TLOGW("ERROR status");
            }break;
            default:{
                TLOGW("unknown status");
            }break;
        }

        if(!pass ){
            // 当前状态不支持的命令/事件

        }
    }
    TLOGD("loop exit");

    jenv->DeleteGlobalRef(mjObjWeakRef);
    mjObjWeakRef = NULL;

}

void Player::notify(JNIEnv *env, int type, int arg1 , int arg2, void *obj)
{
    env->CallStaticVoidMethod(
            gJNIDragonPlayer.thizClass, gJNIDragonPlayer.postEvent,
            mjObjWeakRef , type, arg1, arg2, obj);
}

void Player::sendEvent(EVENT event) {
    AutoMutex _l(mCmdMutex);
    mCmdQueue.push_back(event);
    mCmdCond->signal();
}

void Player::setState(STATE newState){
    TLOGW("updateState %s -> %s" , stateId2Str(mState) , stateId2Str(newState));
    mState = newState ;
}

const char* const Player::stateId2Str(STATE id)
{
    switch(id){
        case UNINIT:
            return "UNINIT";
        case INITED:
            return "INITED";
        case PREPARING:
            return "PREPARING";
        case PREPARED:
            return "PREPARED";
        case PLAYING:
            return "PLAYING";
        case PAUSING:
            return "PAUSING";
        case PAUSED:
            return "PAUSED";
        case STOPPING:
            return "STOPPING";
        case STOPPED:
            return "STOPPED";
        case SEEKING:
            return "SEEKING";
        case ERROR:
            return "ERROR";
        default:
            return "DEFAULT";
    }
}