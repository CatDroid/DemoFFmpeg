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
	AACSWDecoder(AVCodecParameters* para);
	virtual ~AACSWDecoder();


	void start(RenderThread* rt );
	void stop();

	bool put(sp<Buffer> packet);
	void clearupPacketQueue();



private:
	AVCodecContext *mAudioCtx;
	struct SwrContext * mSwrCtx ;
	RenderThread* mpRender ;
	int32_t mDecodedFrameSize ;


	bool mStop ;
	sp<BufferManager> mBM;

	std::list<sp<Buffer>> mPacketQueue;
	Mutex* mQueueMutex ;
	Condition*  mSinkCond ;
	Condition* mSourceCond ;
	pthread_t mDecodeTh ;
	void loop();
	static void* decodeThread(void* arg);


};


#endif /* AACSWDEOCDER_H_ */
