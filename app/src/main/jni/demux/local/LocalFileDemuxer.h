/*
 * LocalFileDemuxer.h
 *
 *  Created on: 2016年12月10日
 *      Author: hanlon
 */

#ifndef LOCALFILEDEMUXER_H_
#define LOCALFILEDEMUXER_H_


#include <pthread.h>
#include <stddef.h>
#include <unistd.h>
#include <string>
#include <list>
#include "DeMuxer.h"
#include "MyPacket.h"

extern "C"{
/*
 * 	编译链接时找不到对应的函数
	undefined reference to `av_register_all()'
	undefined reference to `avformat_open_input(AVFormatContext**, char const*, AVInputFormat*, AVDictionary**)'
	undefined reference to `avformat_find_stream_info(AVFormatContext*, AVDictionary**)'
 */
#include "libavformat/avformat.h"
#include "libavutil/error.h"
}


class LocalFileDemuxer : public DeMuxer
{
private:

	// 线程相关
	pthread_t mExtractThID ;
	bool			mLoop ;
	bool 			mPause;
	bool			mEof ;
	bool 			mParseResult ;

	// 缓存相关
	sp<PacketManager> mPm ;

	// 文件信息/解码参数
	AVFormatContext * mAvFmtCtx;
	int mVstream ;
	int mAstream ;
	std::string     mSPS; // with 00 00 00 01
	std::string     mPPS;
	std::string     mESDS;
	double mVTimebase ;
	double mATimebase ;

	Mutex* mStartMutex;
	Condition* mStartCon;


public :
	LocalFileDemuxer(Player* player);
	virtual ~LocalFileDemuxer();
	virtual const AVCodecParameters* getVideoCodecPara() override ;
	virtual const AVCodecParameters* getAudioCodecPara() override;
	virtual double getVideoTimeBase() override {return mVTimebase ;}
	virtual double getAudioTimeBase() override {return mATimebase ;}

	virtual void prepareAsyn() override ; // player.notify();
	virtual void play() override ;
	virtual void pause()override ;
	virtual void seekTo(int32_t ms) override ;
	virtual void stop() override;
	virtual bool getParseResult() override {return mParseResult;}

private:
	static void* sExtractThread(void *arg);
	bool parseFile(); // 返回false 代表prepare失败了
	void loop() ;
	void setupVideoSpec(bool is_pps , bool addLeaderCode , unsigned char *data, int size);
	void setupAudioSpec(bool addLeaderCode , unsigned char *data, int size);


private:
	CLASS_LOG_DECLARE(LocalFileDemuxer);
};



#endif /* LOCALFILEDEMUXER_H_ */
