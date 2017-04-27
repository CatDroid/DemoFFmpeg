//
// Created by Hanloong Ho on 17-4-6.
//

#ifndef DEMO_FFMPEG_AS_PLAYER_H
#define DEMO_FFMPEG_AS_PLAYER_H


#include <string>
#include <list>
#include <android/native_window.h>
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
        COMPLETE,
        ERROR,
    };
    enum EVENT{
        CMD_UNDEFINE,
        CMD_PREPARE,
        CMD_PREPARE_RESULT,
        CMD_PLAY,
        CMD_PAUSE,
        CMD_PAUSE_COMPLETE,
        CMD_SEEK,
        CMD_STOP,
        CMD_PLAY_COMPLETE,
    };

private:
    jobject mjObjWeakRef ; // 在loop中返回时候释放
    ANativeWindow* mWindow;

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
    Player(JNIEnv* env , jobject jWeakRef);
    virtual ~Player();

    // 外部调用接口
    virtual bool setDataSource(std::string uri);
    virtual bool setView(JNIEnv* env , jobject surface);
    virtual bool prepare();         // 状态不对 返回false
    virtual void prepareSync();
    virtual bool play();            // 状态不对 返回false
    virtual bool pause(); // onPause
    virtual bool seekTo(int32_t ms); // onSeek
    virtual void stop();
    virtual int32_t getDuration(); // ms
    virtual int32_t getCurrent();  // ms
    virtual int32_t getWidth();
    virtual int32_t getHeigth();

    // 内部调用接口
    virtual void prepare_result();  //  Muxer
    virtual void pause_complete();  //  Muxer
    virtual void play_complete();   //  Render


    // 通知应用层接口
    void notify(JNIEnv *env, int type, int arg1 = 0 , int arg2 = 0 , void *obj = NULL);

    // 内部接口
    void _stop();

private:
    static void* sStateMachineTh(void*);
    void loop( void* ctx );
    void sendEvent(EVENT event);
    void setState(STATE newState);
    const char* const stateId2Str(STATE id);

    CLASS_LOG_DECLARE(Player);
};


#endif //DEMO_FFMPEG_AS_PLAYER_H
