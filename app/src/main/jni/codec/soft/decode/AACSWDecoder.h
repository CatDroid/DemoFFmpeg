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
#include "IDeMuxerSink.h"

extern "C"{
#include "libavformat/avformat.h"
#include "libavutil/error.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
}
#include "RenderThread.h"
#include "BufferManager.h"
#include "Buffer.h"
#include "Condition.h"

class AACSWDecoder : public DeMuxerSinkBase
{
public:
	AACSWDecoder(AVCodecParameters* para , double timebase);
	virtual ~AACSWDecoder();


	void start(RenderThread* rt );
	void stop();

	bool put(sp<MyPacket> packet);
	void clearupPacketQueue();



private:
	AVCodecContext *mAudioCtx;
	struct SwrContext * mSwrCtx ;
	RenderThread* mpRender ;
	int32_t mDecodedFrameSize ;
	double mTimeBase ;

	bool mStop ;
	sp<BufferManager> mBM;

	std::list<sp<MyPacket>> mPacketQueue;
	Mutex* mQueueMutex ;
	Condition*  mSinkCond ;
	Condition* mSourceCond ;
	pthread_t mDecodeTh ;
	void loop();
	static void* decodeThread(void* arg);

	//
	sp<SaveFile> mSaveFile ;

};


#endif /* AACSWDEOCDER_H_ */
