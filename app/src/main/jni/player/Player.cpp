//
// Created by Hanloong Ho on 17-4-6.
//



#include "Player.h"

#include "jni_common.h"

#include "DeMuxer.h"
#include "Decoder.h"
#include "LocalFileDemuxer.h"
#include "DecoderFactory.h"

CLASS_LOG_IMPLEMENT(Player,"Player");


Player::Player():mDeMuxer(NULL),mVDecoder(NULL),mADecoder(NULL),mRender(NULL),
        mState(STATE::UNINIT),mStop(false),
        mCmdMutex(NULL),mCmdCond(NULL),
        mSeekToMs(-1){
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
    }else{
        return false ;
    }
}

bool Player::prepare() {
    if(mState == INITED ){
        setState(PREPARING);
        sendEvent(EVENT::CMD_PREPARE);
    }else{
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
        return false ;
    }
    return true ;
}

bool Player::pause() {
    if(mState == PLAYING || mState == PAUSED){
        sendEvent(EVENT::CMD_PAUSE);
    }
    return false ;
}

bool Player::seekTo(int32_t ms) {
    if(mState == PLAYING || mState == PAUSED || mSeekToMs != -1 /*last seek is not complete*/){
        mSeekToMs = ms ;
        sendEvent(CMD_SEEK);
    }else{
        return false ;
    }
}

void Player::stop() {

}

void Player::prepare_result() {


}

Player::~Player()
{
    // 线程退出
}

void* Player::sStateMachineTh(void* ctx )
{
    JNIEnv* env ;
    JNI_ATTACH_JVM_WITH_NAME(env,"Player");
    ((Player*)ctx)->loop(env) ;
    JNI_DETACH_JVM(env);
    return NULL;
}

void Player::loop(void* ctx  )
{
    JNIEnv* jenv = (JNIEnv*)ctx;

    bool pass = false ;

    while(!mStop) {

        EVENT cmd = EVENT::CMD_UNDEFINE ;
        {
            AutoMutex _l(mCmdMutex);
            if( mCmdQueue.empty() ){
                mCmdCond->wait(mCmdMutex);
                continue ;
            }else{
                cmd = mCmdQueue.front();
                mCmdQueue.pop_front();
            }
        }

        if(cmd == EVENT::CMD_UNDEFINE) continue ;
        pass = false ;

        switch(mState){
            case STATE::UNINIT:
                break;
            case STATE::INITED:
                break;
            case STATE::PREPARING:

                if(cmd == EVENT::CMD_PREPARE) {

                    TLOGW("prepare %s ", mPlayURI.c_str());
                    std::size_t pos = mPlayURI.find("://");
                    if (pos == std::string::npos || mPlayURI.size() <= 7) {
                        TLOGE("CMD_PREPARE in PREPARING parse uri error uri:%s size %d ",
                              mPlayURI.c_str(),  mPlayURI.size());
                        setState(STATE::ERROR);
                        // TODO Callback onPrepare ERROR
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
                        if( mDeMuxer->getVideoCodecPara() != NULL){
                            TLOGW("source has Video !");
                            mVDecoder = DecoderFactory::create(mDeMuxer->getVideoCodecPara(),
                                                               mDeMuxer->getVideoTimeBase());
                            if(mVDecoder == NULL){
                                // TODO
                                TLOGE("video decoder create FAIL !");
                            }
                        }
                        setState(STATE::PREPARED);
                        // TODO Callback onPrepare OK
                    }else{
                        // TODO Callback onPrepare ERROR
                        setState(STATE::ERROR);
                    }
                }


                break;
            case STATE::PREPARED:
                if(cmd == EVENT::CMD_PLAY){
                    mDeMuxer->play();
                }

                break;
            case STATE::PLAYING:

                if(cmd == EVENT::CMD_PAUSE){
                    // TODO
                    setState(STATE::PAUSING);
                    pass = true ;
                }else if(cmd == EVENT::CMD_PLAY){
                    // Play And Play again
                    // TODO onNotify
                    pass = true ;
                }
                break;
            case STATE::PAUSING:
                break;
            case STATE::PAUSED:
                break;
            case STATE::STOPPED:
                break;
            case STATE::SEEKING:
                break;
            case STATE::ERROR:
                break;
        }

        if(!pass ){
            // 当前状态不支持的命令/事件

        }
    }

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