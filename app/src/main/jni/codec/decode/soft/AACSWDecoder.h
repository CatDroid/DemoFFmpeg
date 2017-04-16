/*
 * AACSWDeocder.h
 *
 *  Created on: 2016年12月11日
 *      Author: hanlon
 */

#ifndef AACSWDEOCDER_H_
#define AACSWDEOCDER_H_

#include <pthread.h>
#include <list>

#include "Log.h"
#include "SaveFile.h"
#include "Decoder.h"
#include "Render.h"
#include "BufferManager.h"
#include "Buffer.h"
#include "Condition.h"

extern "C"{
#include "libavformat/avformat.h"
#include "libavutil/error.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
}


class AACSWDecoder : public Decoder
{

private:
	// 解码器 和 转换器
	AVCodecContext * mpAudCtx;
	struct SwrContext * mpSwrCtx ;

	// 解码参数
	int32_t mDecodedFrameSize ;
	double mTimeBase ;

	// 待解码packet和生成的buffer
	sp<BufferManager> mBufMgr;
	std::list<sp<MyPacket>> mPktQueue;
	Mutex* mEnqMux ;
	Condition* mEnqSikCnd ;
	Condition* mEnqSrcCnd ;

	// 线程
	bool mStop ;
	pthread_t mEnqThID ;
	pthread_t mDeqThID ;

	// 调试
	sp<SaveFile> mSaveFile ;
public:
	AACSWDecoder();
	virtual ~AACSWDecoder();

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

private:
	CLASS_LOG_DECLARE(AACSWDecoder);

};


#endif /* AACSWDEOCDER_H_ */
