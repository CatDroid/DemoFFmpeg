/*
 * RenderThread.h
 *
 *  Created on: 2016年12月17日
 *      Author: hanlon
 */

#ifndef RENDERTHREAD_H_
#define RENDERTHREAD_H_


extern "C"{
#include "libswscale/swscale.h"
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
}

#include "SurfaceView.h"
#include "AudioTrack.h"
#include <list>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <Condition.h>

#include "SaveFile.h"
#include "Buffer.h"
#include "BufferManager.h"

class RenderThread
{
public:
	RenderThread(SurfaceView* view , AudioTrack* track  );
	~RenderThread();
	void renderAudio(sp<Buffer> buf);
	void renderVideo(sp<Buffer> buf);
	void loop();
	static void* renderloop(void* arg);
private:
	pthread_t mRenderTh;
	SurfaceView* mpView ;
	AudioTrack* mpTrack;
	struct SwsContext *mpSwsCtx;
	AVFrame* mSrcFrame ;
	AVFrame* mDstFrame ;
	int32_t mRGBSize ;
	bool mStop ;
	int64_t mStartSys ;
	int64_t mStartPts ;
	std::list<sp<Buffer>> mAudioRenderQueue;
	std::list<sp<Buffer>> mVideoRenderQueue;
	Mutex* mQueueMutex ;
	Condition* mQueueCond ;
	sp<BufferManager> mBM;

	sp<SaveFile> mDebugFile ;
};




#endif /* RENDERTHREAD_H_ */
