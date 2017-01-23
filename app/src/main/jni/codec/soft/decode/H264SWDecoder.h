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
#include "IDeMuxerSink.h"
#include "MyPacket.h"

extern "C"{
#include "libavformat/avformat.h"
#include "libavutil/error.h"
#include "libavcodec/avcodec.h"
}

#include "RenderThread.h"
#include "Condition.h"
#include "Mutex.h"

class H264SWDecoder : public DeMuxerSinkBase {

public:
	H264SWDecoder(AVCodecParameters* para , double timebase );
	virtual ~H264SWDecoder();

	void start(RenderThread* rt );
	void stop();

	bool put(sp<MyPacket> packet);
	void clearupPacketQueue();

private:
	AVCodecContext *mVideoCtx;

	sp<BufferManager> mBM;
	RenderThread* mpRender ;
	int32_t mDecodedFrameSize ;
	double mTimeBase ;

	std::list<sp<MyPacket>> mPacketQueue;
	Mutex* mQueueMutex ;
	Condition* 	mSinkCond ;
	Condition* 	mSourceCond ;
	pthread_t mDecodeTh ;
	bool mStop ;
	void loop();
	static void* decodeThread(void* arg);

	void just4test()const;
};



#endif /* H264SWDEOCDER_H_ */
