/*
 * RenderThread.h
 *
 *  Created on: 2016年12月17日
 *      Author: hanlon
 */

#ifndef RENDERTHREAD_H_
#define RENDERTHREAD_H_



#include <list>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "ffmpeg_common.h"
#include "Log.h"
#include "SaveFile.h"
#include "Buffer.h"
#include "BufferManager.h"
#include "Render.h"
#include "Condition.h"
#include "SurfaceView.h"
#include "AudioTrack.h"

class RenderThread : public Render
{
public:
	RenderThread(  );
	virtual  ~RenderThread();
	void renderAudio(sp<Buffer> buf);
	void renderVideo(sp<Buffer> buf);
	void loop();
	static void* renderloop(void* arg);
private:

	SurfaceView* mpView ;
	AudioTrack* mpTrack;
	struct SwsContext *mpSwsCtx;
	AVFrame* mSrcFrame ;
	AVFrame* mDstFrame ;
	int32_t mRGBSize ;

	volatile  bool mStop ;
	pthread_t mRenderTh;

	int64_t mStartSys ;
	int64_t mStartPts ;
	std::list<sp<Buffer>> mAudioRenderQueue;
	std::list<sp<Buffer>> mVideoRenderQueue;
	Mutex* mQueueMutex ;
	Condition* mQueueCond ;
	Condition* mFullCond ;
	sp<BufferManager> mBM;

	sp<SaveFile> mDebugFile ;

private:
	CLASS_LOG_DECLARE(RenderThread);
};




#endif /* RENDERTHREAD_H_ */
