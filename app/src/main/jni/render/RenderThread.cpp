/*
 * RenderThread.cpp
 *
 *  Created on: 2016年12月17日
 *      Author: hanlon
 */
#include "RenderThread.h"

#include <sys/prctl.h>

CLASS_LOG_IMPLEMENT(RenderThread,"RenderThread");

#define VIDEO_RENDER_BUFFER_SIZE 20

RenderThread::RenderThread(  ):
		mpView(NULL),mpTrack(NULL),mpSwsCtx(NULL),
		mSrcFrame(NULL),mDstFrame(NULL),
		mStop(false),
		mRenderTh(-1),
		mStartSys(-1),mStartPts(-1)
{

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

		mpSwsCtx = sws_getContext(buf->width(), buf->height(), (AVPixelFormat)buf->fmt(),/*AV_PIX_FMT_YUV420P*/
								  buf->width(), buf->height(), AV_PIX_FMT_RGBA,
									SWS_FAST_BILINEAR,
									0, 0, 0);
		mSrcFrame = av_frame_alloc();
		mDstFrame = av_frame_alloc();

		mRGBSize = av_image_get_buffer_size( AV_PIX_FMT_RGBA , buf->width(), buf->height(), 1 ); // 1是linesize的对齐align
		mBM = new BufferManager( mRGBSize,  30 );
	}

	sp<Buffer> rgbbuf = mBM->pop();

	if(buf == NULL){// End Of File
		TLOGW("RenderThread get End oF File");
		rgbbuf = NULL ;
	}else{
		/*
	 * av_image_xxx 系列 比  avpicture_xxx系列 增加了 linesize的alian对齐参数
	 */
		av_image_fill_arrays(mSrcFrame->data, mSrcFrame->linesize, buf->data() , (AVPixelFormat)buf->fmt() /*AV_PIX_FMT_YUV420P*/, buf->width(), buf->height(), 1);
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

		TLOGT("output slice height %d ,  RGBA height %d " , ret , buf->height() );
		//TLOGD("linesize[0] = %d , mRGBSize = %d " ,mDstFrame->linesize[0] * buf->height() , mRGBSize );

		rgbbuf->width() = buf->width();
		rgbbuf->height() = buf->height() ;
		rgbbuf->pts() = buf->pts();
		rgbbuf->size() = mRGBSize ;
	}


	while(!mStop){
		AutoMutex l(mQueueMutex);
		bool empty = mVideoRenderQueue.empty();
		if(empty){
			mVideoRenderQueue.push_back(rgbbuf);
			mQueueCond.signal();
		}else{
			if( mVideoRenderQueue.size() >= VIDEO_RENDER_BUFFER_SIZE ){
				TLOGW("too much video RenderBuffer wait!");
				mFullCond.wait(mQueueMutex);
				continue ;
			}else{
				mVideoRenderQueue.push_back(rgbbuf);
			}
		}
		break;
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
			if( ! mVideoRenderQueue.empty()  )
			{
				int size = mVideoRenderQueue.size();
				vbuf = mVideoRenderQueue.front();
				mVideoRenderQueue.pop_front();
				if( size >= VIDEO_RENDER_BUFFER_SIZE ) mFullCond.signal();
			}else{
				if( mStop ) break;
				mQueueCond.wait(mQueueMutex);
				continue;
			}
		}

		if( vbuf == NULL) {
			TLOGW("video render thread: End Of File !");
			break;
		}

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
		TLOGD("draw w %d h %d size %d pts[A:%" PRId64 " V:%" PRId64 " ] A-V %" PRId64 " ms " ,  vbuf->width(), vbuf->height() , vbuf->size() ,mpTrack->pts() ,  vbuf->pts() , (mpTrack->pts() - vbuf->pts()) );
		mpView->draw(vbuf->data() , vbuf->size() ,vbuf->width(), vbuf->height() );

	}

	if(mpTrack == NULL) return ; // TODO
	while( !mStop && !mpTrack->isStoped() ){
		TLOGW("video render thread wait for audio render thread mStop = %d isStoped = %d " , mStop , mpTrack->isStoped());
		usleep(10*1000);
	}
	TLOGW("video render thread Exit mStop = %d isStoped = %d " , mStop , mpTrack->isStoped());

}

void* RenderThread::renderloop(void* arg)
{
	prctl(PR_SET_NAME,"MyRenderThread");
	RenderThread* pthread = (RenderThread*)arg;
	pthread->loop() ;
	return NULL;
}



void RenderThread::start() {
	int ret = ::pthread_create(&mRenderTh , NULL ,  renderloop , this );
	if( ret != 0 ){
		mRenderTh = -1 ;
		TLOGE("pthread_create ERROR %d %s " , errno ,strerror(errno));
	}
}

void RenderThread::pause() {
	//TODO 暂停播放
}

void RenderThread::stop() {
	mStop = true ;
	if( mRenderTh != -1 ){
		{
			AutoMutex l(mQueueMutex);
			mQueueCond.signal();
			mFullCond.signal();
		}
		TLOGD("~RenderThread Join ");
		::pthread_join(mRenderTh , NULL );
		mRenderTh = -1;
	}
}


RenderThread::~RenderThread()
{
	if( mpSwsCtx != NULL ) {
		sws_freeContext(mpSwsCtx);
		av_frame_free(&mSrcFrame);
		av_frame_free(&mDstFrame);
	}
	TLOGD("~RenderThread done ");
}


