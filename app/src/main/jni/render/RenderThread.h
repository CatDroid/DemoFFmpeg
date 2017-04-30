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

private:
	// 音频相关
	AudioTrack* mpTrack;
 	// 视频相关
	SurfaceView* mpView ;
	struct SwsContext *mpSwsCtx;
	AVFrame* mSrcFrame ;
	AVFrame* mDstFrame ;
	int32_t mRGBSize ;
	// 线程相关
	volatile  bool mStop ;
	volatile bool mPause ;
	Mutex mPauseMutex;
	Condition mPauseCon;
	pthread_t mRenderTh;

	// 同步相关
	int64_t mStartSys ;		// 记录第一个视频/音频包播放时的系统启动时间 us
	int64_t mVidStartPts ;	// 记录第一个视频时间
	int64_t mAudStartPts ;  // 记录第一个音频时间
	bool mVBufingDone ;		// 视频解码满 一开始需要先缓冲5帧


#define VIDEO_BUFFERING_BEFORE_PLAY 5

	// 缓冲队列相关
	std::list<sp<Buffer>> mAudRdrQue; // Audio Render Queue
	std::list<sp<Buffer>> mVidRdrQue; // Video Render Queue
	Mutex mQueMtx ;			// 保护两个队列
	Condition mVQueCond ;	// 视频队列已经有空位
	Condition mAQueCond ;	// 音频队列已经有空位
	Condition mSrcCond ;	// 有新的数据加入Audio/Video队列
	bool mVidFlush;
	bool mAudFlush; 		// 在队列满等待时候 出现了seek/flush
	bool mLoopFlush ;
	sp<BufferManager> mRgbBm;

	// 调试
	sp<SaveFile> mDebugFile ;
	int64_t mLastDVTime ;
	int64_t mLastDATIme ;

public:
	RenderThread(Player* player );
	virtual  ~RenderThread() ;

	void loop();
	static void* renderThreadEntry(void *arg);


	// Render接口
	void renderAudio(sp<Buffer> buf) override;
	void renderVideo(sp<Buffer> buf) override;
	void flush(bool isVideo) override;
	void start() override ;
	void stop() override ;
	void pause() override ;
	void play() override ;
	int32_t getCurrent() override ;

private:
	CLASS_LOG_DECLARE(RenderThread);
};




#endif /* RENDERTHREAD_H_ */
