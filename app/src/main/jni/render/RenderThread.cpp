/*
 * RenderThread.cpp
 *
 *  Created on: 2016年12月17日
 *      Author: hanlon
 */

#define LOG_TAG "RenderThread"


#include "jni_common.h"

#include "RenderThread.h"


RenderThread::RenderThread(SurfaceView* view , AudioTrack* track ):
									mpView(view),mpTrack(track),mStop(false),
									mpSwsCtx(NULL),mSrcFrame(NULL),mDstFrame(NULL),mRenderTh(-1),
									mStartSys(-1),mStartPts(-1)
{


	mQueueMutex = new Mutex();
	mQueueCond = new Condition();

	int ret = ::pthread_create(&mRenderTh , NULL ,  renderloop , this );
	if( ret != 0 ){
		mRenderTh = -1 ;
		ALOGE("pthread_create ERROR %d %s " , errno ,strerror(errno));
	}

}

RenderThread::~RenderThread()
{
	mStop = true ;
	if( mRenderTh != -1 ){
		{
			AutoMutex l(mQueueMutex);
			mQueueCond->signal();
		}
		ALOGD("~RenderThread Join ");
		::pthread_join(mRenderTh , NULL );
		mRenderTh = -1;
		delete mQueueMutex; mQueueMutex = NULL;
		delete mQueueCond; mQueueCond = NULL;
	}
	if( mpSwsCtx != 0 ) {
		sws_freeContext(mpSwsCtx);
		av_frame_free(&mSrcFrame);
		av_frame_free(&mDstFrame);
	}
	ALOGD("~RenderThread done ");
}

void RenderThread::renderAudio(sp<Buffer> buf)
{
//	if( mDebugFile.get() == NULL ){
//		mDebugFile = new SaveFile("/mnt/sdcard/render_audio.pcm");
//	}
//	mDebugFile->save(buf->data(),buf->size());

	mpTrack->write(buf);
}

void RenderThread::renderVideo(sp<Buffer> buf)
{
	if( mpSwsCtx == NULL ){

		mpSwsCtx = sws_getContext(buf->width(), buf->height(), AV_PIX_FMT_YUV420P,
								  buf->width(), buf->height(), AV_PIX_FMT_RGBA,
									SWS_FAST_BILINEAR,
									0, 0, 0);
		mSrcFrame = av_frame_alloc();
		mDstFrame = av_frame_alloc();

		mRGBSize = av_image_get_buffer_size( AV_PIX_FMT_RGBA , buf->width(), buf->height(), 1 );
		mBM = new BufferManager( mRGBSize,  30 );
	}

	sp<Buffer> rgbbuf = mBM->pop();

	av_image_fill_arrays(mSrcFrame->data, mSrcFrame->linesize, buf->data() , AV_PIX_FMT_YUV420P, buf->width(), buf->height(), 1);
	av_image_fill_arrays(mDstFrame->data, mDstFrame->linesize, rgbbuf->data() , AV_PIX_FMT_RGBA, buf->width(), buf->height(), 1);


	/*
		 * @param c         上下文 sws_getContext()
		 * @param srcSlice  数组 包含源slice的各个平面的指针
		 * @param srcStride 数组 包含源image的各个平面的步幅
		 * @param srcSliceY 将要处理slice在源image的位置 也就是 image第一行的位置(从0开始)
		 * @param srcSliceH 源slice的高度, 也就是slice的行数
		 * @param dst       数组 包含目标image的各个平面的指针
		 * @param dstStride 数组 包含目标image的各个平面的步幅(大小? stride)
		 * @return          输出的高度? the height of the output slice
		 */
	int ret  = sws_scale( mpSwsCtx,
						  mSrcFrame->data,
						  mSrcFrame->linesize,
						  0,
						  buf->height(),
						  mDstFrame->data, mDstFrame->linesize );
	// 不需要把AVFrame mDstFrame 进行 av_image_copy_to_buffer 因为RGBA只有一个平面 已经转换到了目标Buffer
	//if( ret != height ){
	//	ALOGE("sws_scale result %d ", ret ); // ????
	//}

	//ALOGD("linesize[0] = %d , mRGBSize = %d " ,mDstFrame->linesize[0] * buf->height() , mRGBSize );
	rgbbuf->width() = buf->width();
	rgbbuf->height() = buf->height() ;
	rgbbuf->pts() = buf->pts();
	rgbbuf->size() = mRGBSize ;

	{
		AutoMutex l(mQueueMutex);
		bool empty = mVideoRenderQueue.empty();
		mVideoRenderQueue.push_back(rgbbuf);
		if(empty) mQueueCond->signal();
	}

}


int64_t getCurTimeMs()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (uint64_t)(now.tv_sec * 1000UL + now.tv_nsec / 1000000UL);
}


void RenderThread::loop()
{
	while( ! mStop ){
		sp<Buffer> vbuf = NULL;
		{
			AutoMutex l(mQueueMutex);
			if( ! mVideoRenderQueue.empty()   )
			{
				vbuf = mVideoRenderQueue.front();
				mVideoRenderQueue.pop_front();
			}else{
				if( mStop ) break;
				mQueueCond->wait(mQueueMutex);
				continue;
			}
		}

		if( vbuf == NULL) continue ;

		int64_t now = getCurTimeMs()  ;
		if( mStartSys == -1 ){
			mStartSys = now ;
			mStartPts = vbuf->pts() ;
		}else{

			int64_t play_time = mStartSys + ( vbuf->pts() - mStartPts );
			int64_t delay = play_time - now  ;
			if( delay > 0){
				usleep( delay *1000 );
			}else{
				// drop ????
			}
		}

		//if( vbuf == NULL  ){
		//	usleep(5000); // 23ms@   16ms@60fps
		//}
		ALOGD("draw w %d h %d pts %lld size %d A-V %f" ,  vbuf->width(), vbuf->height() , vbuf->pts() , vbuf->size() , (mpTrack->pts() - vbuf->pts()) );
		mpView->draw(vbuf->data() , vbuf->size() ,vbuf->width(), vbuf->height() );



	}
}

void* RenderThread::renderloop(void* arg)
{
	RenderThread* pthread = (RenderThread*)arg;
	pthread->loop() ;
	return NULL;
}


