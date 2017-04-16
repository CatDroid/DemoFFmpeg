//
// Created by Hanloong Ho on 17-4-6.
//

#ifndef DEMO_FFMPEG_AS_PLAYER_H
#define DEMO_FFMPEG_AS_PLAYER_H


#include <string>
#include <list>
#include "Log.h"
#include "Condition.h"

class Decoder;
class DeMuxer;
class Render;


class Player {

public:
    enum STATE{
        UNINIT ,
        INITED,
        PREPARING,
        PREPARED,
        PLAYING,
        PAUSING,
        PAUSED,
        STOPPED,
        STOPPING,
        SEEKING,
        ERROR,
    };
    enum EVENT{
        CMD_UNDEFINE,
        CMD_PREPARE,
        CMD_PREPARE_RESULT,
        CMD_PLAY,
        CMD_PAUSE,
        CMD_SEEK,
        CMD_STOP,
        CMD_PLAY_COMPLETE,
    };

private:
    jobject mjObjWeakRef ; // move out

    DeMuxer* mDeMuxer ;
    Decoder* mVDecoder ;
    Decoder* mADecoder ;
    Render*  mRender ;

    STATE mState  ;
    bool mStop ;
    std::list<EVENT> mCmdQueue;
    Mutex* mCmdMutex;
    Condition* mCmdCond;
    pthread_t mThreadID;


    std::string mPlayURI;
    int32_t mSeekToMs ;

public:
    Player(jobject jWeakRef);
    virtual ~Player();

    // 外部调用接口
    virtual bool setDataSource(std::string uri);
    virtual bool prepare();         // 状态不对 返回false
    virtual void prepareSync();
    virtual bool play();            // 状态不对 返回false
    virtual bool pause(); // onPause
    virtual bool seekTo(int32_t ms); // onSeek
    virtual void stop();

    // 内部调用接口
    virtual void prepare_result();

    // 通知应用层接口
    void notify(JNIEnv *env, int type, int arg1 = 0 , int arg2 = 0 , void *obj = NULL);

private:
    static void* sStateMachineTh(void*);
    void loop( void* ctx );
    void sendEvent(EVENT event);
    void setState(STATE newState);
    const char* const stateId2Str(STATE id);

    CLASS_LOG_DECLARE(Player);
};


#endif //DEMO_FFMPEG_AS_PLAYER_H
