//
// Created by Hanloong Ho on 17-4-9.
//

#ifndef DEMO_FFMPEG_AS_DEMUXER_H
#define DEMO_FFMPEG_AS_DEMUXER_H


#include <string>


extern "C" {
#include "libavcodec/avcodec.h"
};

#include "Player.h"

class DeMuxer {

protected:
    Player*  mPlayer ;
    Decoder* mADecoder;
    Decoder* mVDecoder;
    std::string mUri ;

public:
    DeMuxer(Player* player):mPlayer(player),mADecoder(NULL),mVDecoder(NULL){ }
    virtual ~DeMuxer(){ }

    // 控制
    virtual bool setDataSource(std::string path) { mUri = path; return true ; }
    virtual void prepareAsyn() = 0 ; // player.notify();
    virtual void play() = 0 ;
    virtual void pause()= 0 ;
    virtual void seekTo(int32_t ms) = 0 ;
    virtual void stop() = 0 ;
    virtual bool getParseResult() = 0 ;

    // 消费者
    virtual void setAudioDecoder(Decoder* codec) { mADecoder = codec ;}
    virtual void setVideoDecoder(Decoder* codec) { mVDecoder = codec ;}

    // 参数 在prepare完成之后 应该可以获取如下参数(sps pps vps可能以帧方式提供)
    virtual const char* getSps() = 0 ; // SPS (Sequence Parameter Sets*)
    virtual const char* getPps() = 0 ; // PPS (Picture Parameter Sets*)
    virtual const char* getVps() = 0 ; // VPS (Video Parameter Sets*)
    virtual double getVideoTimeBase() = 0 ;
    virtual double getAudioTimeBase() = 0 ;
    virtual const AVCodecParameters* getAudioCodecPara() = 0 ;
    virtual const AVCodecParameters* getVideoCodecPara() = 0 ;

};


#endif //DEMO_FFMPEG_AS_DEMUXER_H
