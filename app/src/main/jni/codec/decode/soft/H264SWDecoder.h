/*
 * H264SWDeocder.h
 *
 *  Created on: 2016年12月11日
 *      Author: hanlon
 */

#ifndef H264SWDEOCDER_H_
#define H264SWDEOCDER_H_

#include <pthread.h>
#include <list>

#include "Decoder.h"
#include "MyPacket.h"
#include "Log.h"
#include "Condition.h"
#include "Mutex.h"
#include "ffmpeg_common.h"



class H264SWDecoder : public Decoder {

private:
	// 解码器
	AVCodecContext *mpVidCtx;

	// 解码参数
	int32_t mDecodedFrameSize ; // 解码后数据的大小
	double mTimeBase ;			// 时间基准 由AvFormatContext的AVStream提供

	// 待解码packet和生成的buffer
	sp<BufferManager> mBufMgr;
	std::list<sp<MyPacket>> mPktQueue;
	Mutex* mEnqMux ;
	Condition* 	mEnqSikCnd ;
	Condition* 	mEnqSrcCnd ;
	Mutex mSndRcvMux ;

	// 线程
	pthread_t mEnqThID ;
	pthread_t mDeqThID ;
	bool mStop ;

public:
	H264SWDecoder();
	virtual ~H264SWDecoder();

	// Decoder接口
	bool init(const AVCodecParameters* para , double timebase ) override ;
	void start() override ;
	void stop() override ;
	bool put(sp<MyPacket> packet, bool wait) override ;

private:
	// 线程
	void enqloop(); // Feeding enqloop
	void deqloop(); // Draining enqloop
	static void* enqThreadEntry(void *arg);
	static void* deqThreadEntry(void* arg);
	void clearupPacketQueue();

	// 只是测试
	void just4test()const;

private:
	CLASS_LOG_DECLARE(H264SWDecoder);
};



#endif /* H264SWDEOCDER_H_ */
