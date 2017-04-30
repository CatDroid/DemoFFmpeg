//
// Created by Hanloong Ho on 17-4-9.
//

#ifndef DEMO_FFMPEG_AS_RENDER_H
#define DEMO_FFMPEG_AS_RENDER_H


#include "Player.h"
#include "Buffer.h"

class Render {

protected:
    // 引用Player对象
    Player* mPlayer ;

    // 音频参数
    int mAChannls;
    int mASampleRate ;
    int mABits;

    // 视频参数
    int mVFormat ;
    int mVWidth;
    int mVHeight;

    // 屏幕 View
    void* mNWin ;

public:
    Render(Player* player):mPlayer(player),
            mAChannls(-1),mASampleRate(-1),mABits(-1),
            mVFormat(-1),mVWidth(-1),mVHeight(-1){}
    virtual ~Render() {} ;
    virtual void setAudioParam(int channel , int sample_rate, int bits){
        mAChannls = channel; mASampleRate = sample_rate; mABits = bits ;
    }
    virtual void setVideoParam(int format , int width , int height ){
        mVFormat = format ; mVWidth = width ; mVHeight = height ;
    }
    virtual void setPreview(void* native_view) { mNWin = native_view; }

    /*
     * 解码线程推送解码后的视频和音频
     * 分别有两个队列/缓冲 如果满的话 就会阻塞
     */
    virtual void renderAudio(sp<Buffer> buf) = 0;
    virtual void renderVideo(sp<Buffer> buf) = 0;

    /*
     * 1.根据参数(setAudioParam/setVideoParam)创建Ttack和View
     * 2.启动同步渲染线程
     */
    virtual void start() = 0 ;

    /*
     * 1.唤醒等待在 视频/音频 渲染队列的 Decoder deqTh线程
     * 2.退出同步渲染线程
     */
    virtual void play() = 0 ; // 第一次启动start()之后play(),类似resume()
    virtual void stop() = 0 ;
    virtual void pause() = 0 ;

    /*
     * seek之后
     */
    virtual void flush(bool isVideo) = 0 ;

    /*
     * 1.获得当前播放进度 (ms)
     * 2.目前是优先使用音频的 没有音频使用视频
     */
    virtual int32_t getCurrent() = 0 ;
};


#endif //DEMO_FFMPEG_AS_RENDER_H
