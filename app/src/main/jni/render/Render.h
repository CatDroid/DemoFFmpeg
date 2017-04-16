//
// Created by Hanloong Ho on 17-4-9.
//

#ifndef DEMO_FFMPEG_AS_RENDER_H
#define DEMO_FFMPEG_AS_RENDER_H


#include "Buffer.h"

class Render {

private:
    int mAChannls;
    int mASampleRate ;
    int mABits;
    int mVFormat ;
    int mVWidth;
    int mVHeight;
    void* mNWin ;
public:
    Render():
            mAChannls(-1),mASampleRate(-1),mABits(-1),
            mVFormat(-1),mVWidth(-1),mVHeight(-1){}
    virtual ~Render() {} ;
    virtual void setAudioParam(int channel , int sample_rate, int bits){
        mAChannls = channel; mASampleRate = sample_rate; mABits = bits ;
    }
    virtual void setVideoParam(int format , int width , int height ){
        mVFormat = format ; mVWidth = width ; mVHeight = height ;
    }
    virtual void renderAudio(sp<Buffer> buf) = 0;
    virtual void renderVideo(sp<Buffer> buf) = 0;
    virtual void setPreview(void* native_view) { mNWin = native_view; }
};


#endif //DEMO_FFMPEG_AS_RENDER_H
