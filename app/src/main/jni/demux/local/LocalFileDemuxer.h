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
	bool			mStop ;
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
	int32_t mDuration ;

	// 启动 暂停
	Mutex* mStartMutex;
	Condition* mStartCon;

	// Seek
	Mutex 	mSeekMutex ;
	bool 	mSeekResult ;

public :
	LocalFileDemuxer(Player* player);
	virtual ~LocalFileDemuxer();
	virtual const AVCodecParameters* getVideoCodecPara() override ;
	virtual const AVCodecParameters* getAudioCodecPara() override;
	virtual double getVideoTimeBase() override {return mVTimebase ;}
	virtual double getAudioTimeBase() override {return mATimebase ;}

	virtual void prepareAsyn() override ; // player.notify();
	virtual bool getParseResult() override {return mParseResult;}
	virtual void play() override ;
	virtual void pause()override ;
	virtual int32_t getDuration() override ; // ms
	virtual void seekTo(int32_t ms) override ;
	virtual bool getSeekResult() override { return mSeekResult ; }
	virtual void stop() override;


public:
	//无论是SPS PPS VPS ESDS都带有引导头 00 00 00 01
	virtual const char *getSps() override {
		return mSPS.c_str();
	}

	virtual const char *getPps() override {
		return mPPS.c_str();
	}

	virtual const char *getVps() override {
		// TODO
		return NULL;
	}

private:

	static void* exThreadEntry(void *arg); // extract
	bool parseFile(); // 返回false 代表prepare失败了
	void loop() ;

	//无论是SPS PPS VPS ESDS都带有引导头 00 00 00 01
	void setupVideoSpec(bool is_pps , bool addLeaderCode , unsigned char *data, int size);
	void setupAudioSpec(bool addLeaderCode , unsigned char *data, int size);

private:
	CLASS_LOG_DECLARE(LocalFileDemuxer);
};



#endif /* LOCALFILEDEMUXER_H_ */
