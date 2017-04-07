//
// Created by Hanloong Ho on 17-4-6.
//

#ifndef DEMO_FFMPEG_AS_PLAYER_H
#define DEMO_FFMPEG_AS_PLAYER_H


#include <string>

class Player {

private:
    MediaDecoder* mDecoder ;
    Render* mRender ;

public:
    enum STATE{
        UNINIT ,
        INITED,
        PREPARING,
        PREPARED,
        PLAYING,
        PAUSING,
        STOPED,
        ERROR,
    };
    Player():mDecoder(NULL),mRender(NULL){}
    virtual ~Player();
    virtual void start();
    virtual bool setDataSource();
    virtual void perpare();
    virtual void prepareSync(){}; // onPrepare
    // 1.立刻返回
    // 2.
    virtual void play(); //
    virtual void pause(); // onPause
    virtual void seekTo(); // onSeek
    virtual void stop();

};


#endif //DEMO_FFMPEG_AS_PLAYER_H
