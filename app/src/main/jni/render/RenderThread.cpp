/*
 * RenderThread.cpp
 *
 *  Created on: 2016年12月17日
 *      Author: hanlon
 */
#include "RenderThread.h"

#include <sys/prctl.h>
#include "project_utils.h"

CLASS_LOG_IMPLEMENT(RenderThread,"RenderThread");

#define VIDEO_RENDER_BUFFER_SIZE 30
#define AUDIO_RENDER_BUFFER_SIZE 46



RenderThread::RenderThread(  ):mpTrack(NULL),
		mpView(NULL),mpSwsCtx(NULL),
		mSrcFrame(NULL),mDstFrame(NULL),mRGBSize(0),
		mStop(false),  mRenderTh(-1),
		mStartSys(-1),mVidStartPts(-1),mAudStartPts(-1),mVBufingDone(false)
{
	TLOGT("RenderThread");
	  mLastDVTime = 0 ;
	  mLastDATIme = 0 ;
}


void RenderThread::renderAudio(sp<Buffer> buf)
{
//	if( mDebugFile.get() == NULL ){
//		mDebugFile = new SaveFile("/mnt/sdcard/render_audio.pcm");
//	}
//	mDebugFile->save(buf->data(),buf->size());

	while(!mStop){
		AutoMutex l(mQueMtx);
		if( mAudRdrQue.size() >= AUDIO_RENDER_BUFFER_SIZE  ){
			TLOGW("too much audio RenderBuffer wait!");
			mAQueCond.wait(mQueMtx);
			continue ;
		}else{
			TLOGT("pending Audio Queue Size %lu before", mAudRdrQue.size( ));
			mAudRdrQue.push_back(buf);
			if(mAudRdrQue.size() == 1){
				mSrcCond.signal();
			}
		}
		break;
	}
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
		assert( mRGBSize > 0 );
		mRgbBm = new BufferManager("rgb", (uint32_t) mRGBSize, VIDEO_RENDER_BUFFER_SIZE );
	}


	while(!mStop){
		{
			AutoMutex l(mQueMtx);
			if( mVidRdrQue.size() >= VIDEO_RENDER_BUFFER_SIZE  ){
				TLOGW("too much video RenderBuffer wait!");
				mVQueCond.wait(mQueMtx);
				continue ;
			}
			TLOGT("pending Video Queue Size %lu before", mVidRdrQue.size( ));
			//  mVidRdrQue.size() + SurfaceView.mDisBuf.size() == mRgbBm.mTotalBuffers
			//  如果不能及时显示  SurfaceView.mDisBuf.size()可能增大,
			//  导致 虽然mVidRdrQue不超过VIDEO_RENDER_BUFFER_SIZE=30 但是 mRgbBm.mTotalBuffers 可能超过 30
			//  结果内存越来越大 所以要提高SurfaceView.loop的级别为-4
			//  所以如果内存过大 要检查SurfaceView.mDisBuf的数量
		}

		sp<Buffer> rgbbuf = mRgbBm->pop();

		if(buf->size() == -1 ){// 结束时候 H264SWDecoder.cpp往这里放送 =-1 的Buffer
			TLOGW("RenderThread get End oF File [%d %d %d %" PRId64 "]", buf->width() ,buf->height(),buf->size(),buf->pts());
			rgbbuf->size() = -1 ;
		}else{
			CostHelper cost ;
			/*
             * av_image_xxx 系列 比  avpicture_xxx系列 增加了 linesize的alian对齐参数
             */
			av_image_fill_arrays(mSrcFrame->data, mSrcFrame->linesize, buf->data() , (AVPixelFormat)buf->fmt() /*AV_PIX_FMT_YUV420P*/, buf->width(), buf->height(), 1);
			av_image_fill_arrays(mDstFrame->data, mDstFrame->linesize, rgbbuf->data() , AV_PIX_FMT_RGBA, buf->width(), buf->height(), 1);

			// 耗时点: <21ms@1080P
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

//			int64_t now  = ::getCurTimeUs();
//			int64_t diff = now - mLastDVTime;// us
//			if( diff > 16000) TLOGW("video decoder too slow ! %" PRId64 " us", diff-16000);
//			mLastDVTime = now ;// 可能受到这里pending的影响 (mVidRdrQue.size() >= VIDEO_RENDER_BUFFER_SIZE)

			TLOGT("output slice height %d ,  RGBA height %d cost %" PRId64 , ret , buf->height(),cost.Get() );

			//TLOGD("linesize[0] = %d , mRGBSize = %d " ,mDstFrame->linesize[0] * buf->height() , mRGBSize );

			rgbbuf->width() = buf->width();
			rgbbuf->height() = buf->height() ;
			rgbbuf->pts() = buf->pts();
			rgbbuf->size() = mRGBSize ;
		}

		{
			AutoMutex l(mQueMtx);
			mVidRdrQue.push_back(rgbbuf);
			if(mVidRdrQue.size() == 1 || !mVBufingDone){
				mSrcCond.signal();
			}
			TLOGT("pending Video Queue Size %lu after", mVidRdrQue.size( ));
		}

		break;
	}

}





void RenderThread::loop()
{
	bool audio_end = false ;
	bool video_end = false ;
	TLOGD("RenderThread entry");

	sp<Buffer> vbuf = NULL;
	sp<Buffer> abuf = NULL;

	while( !mStop && !(audio_end&&video_end) ){

		{
			AutoMutex l(mQueMtx);
			if( vbuf.get() == NULL && ! mVidRdrQue.empty()  ) {
				vbuf = mVidRdrQue.front();
				mVidRdrQue.pop_front();
				mVQueCond.signal();
				if (vbuf->size() == -1 ){
					vbuf = NULL;
					video_end = true ;
					TLOGW("video end");
				}
				TLOGT("get Video %lu", mVidRdrQue.size());
			}

			if( abuf.get() == NULL && ! mAudRdrQue.empty() ){
				abuf = mAudRdrQue.front();
				mAudRdrQue.pop_front();
				mAQueCond.signal();
				if (abuf->size() == -1 ){
					abuf = NULL;
					audio_end = true ;
					TLOGW("audio end");
				}
				TLOGT("get Audio %lu", mAudRdrQue.size() );
			}

			if(mVidStartPts == -1 || mAudStartPts ==-1 ){
				if(vbuf.get() == NULL || abuf.get() == NULL ||
						mVidRdrQue.size() < VIDEO_BUFFERING_BEFORE_PLAY /*buffering*/ ){
					// VIDEO_RENDER_BUFFER_SIZE
					// 必须等待视频第一帧过来才开始播放 否者音频会过早播放(音频解码快 而且不像H264这样会有'解码延迟')
					TLOGW("sync wait for video and audio, video queue %lu " , mVidRdrQue.size());
					mSrcCond.wait(mQueMtx);
					continue ;
				}
				mVBufingDone = true;
			}
			if(vbuf.get() == NULL && abuf.get() == NULL ){
				mSrcCond.wait(mQueMtx);
				continue ;
			}
		}


		int64_t nowus = getCurTimeUs()  ;

		if( mVidStartPts == -1 && vbuf.get() != NULL ) mVidStartPts = vbuf->pts();
		if( mAudStartPts == -1 && abuf.get() != NULL ) mAudStartPts = abuf->pts();
		if( mStartSys == -1 ) mStartSys = nowus;


		TLOGT("vs=%" PRId64 ",as=%" PRId64 ",vbuf=%p(%" PRId64 "),abuf=%p(%" PRId64 "),pos=%" PRId64 " us" ,
			  mVidStartPts , mAudStartPts ,
			  vbuf.get() , (vbuf.get()==NULL)?-111:vbuf.get()->pts(),
			  abuf.get() , (abuf.get()==NULL)?-111:abuf.get()->pts(),
			  nowus - mStartSys );


		uint8_t rwho = 0 ; // 0:audio 1:video 2:both
		int64_t delayus = 0 ;
		if(vbuf.get() != NULL && abuf.get() != NULL ){

			int64_t delay_vtime = mStartSys + ( vbuf->pts() - mVidStartPts ) - nowus;
			int64_t delay_atime = mStartSys + ( abuf->pts() - mAudStartPts ) - nowus;
			if( delay_vtime <= 0 && delay_atime <= 0 ){
				rwho = 2 ; delayus = (delay_vtime>delay_atime)?delay_atime:delay_vtime ;
			}else if( delay_vtime < delay_atime){
				rwho = 1 ; delayus = delay_vtime ;
			}else{
				rwho = 0 ; delayus = delay_atime ;
			}
		}else if(NULL != vbuf.get()){
			rwho = 1 ;
			delayus = mStartSys + ( vbuf->pts() - mVidStartPts ) - nowus;
		}else {
			rwho = 0 ;
			delayus = mStartSys + ( abuf->pts() - mAudStartPts ) - nowus;
		}


		if( delayus > 1000 ){ // 1ms
			{
				TLOGT("wait %" PRId64 " us for %d " ,((uint64_t) delayus - 1000), rwho);
				AutoMutex l(mQueMtx);
				mSrcCond.waitRelative(mQueMtx , (((uint64_t) delayus - 1000) * 1000));
			}
			continue ;
		}else if(delayus < -5000 ){
			TLOGE("%d too lateny %" PRId64 " us !" , rwho ,delayus );
		}


		if( rwho == 0 || rwho == 2){
			TLOGD("write audio size %d pts %" PRId64 , abuf->size() , abuf->pts());
			mpTrack->write(abuf);
			abuf = NULL ;
		}

		if( rwho == 1 || rwho == 2 ){
			TLOGD("draw buffer %p w %d h %d size %d pts[A:%" PRId64 " V:%" PRId64 " ] A-V %" PRId64 " us " ,
				  vbuf.get(), vbuf->width(), vbuf->height() , vbuf->size() ,
				  mpTrack->pts() ,  vbuf->pts() , (mpTrack->pts() - vbuf->pts()) );
			if(mpView == NULL){
				TLOGW("window is NOT set, but get VIDEO frame!");
			}else{
//				int64_t before = getCurTimeUs()  ;
				//TLOGT("before draw");
				// Draw相当耗时 在16ms左右 可能跟VSYNC 屏幕刷新60有关
				mpView->draw(vbuf);
//				TLOGT("after draw  %" PRId64 " us" ,(getCurTimeUs()-before) );
			}
			vbuf = NULL;
		}

	}

	if(video_end && audio_end ){
		//TODO 通知应用层播放完毕
	}

//	if(mpTrack == NULL) return ; // TODO
//	while( !mStop && !mpTrack->isStoped() ){
//		TLOGW("video render thread wait for audio render thread mStop = %d isStoped = %d " , mStop , mpTrack->isStoped());
//		usleep(10*1000);
//	}
//	TLOGW("video render thread Exit mStop = %d isStoped = %d " , mStop , mpTrack->isStoped());

	TLOGD("RenderThread exit");
}

void* RenderThread::renderThreadEntry(void *arg)
{
	prctl(PR_SET_NAME,"MyRenderThread");
	RenderThread* pthread = (RenderThread*)arg;
	pthread->loop() ;
	return NULL;
}



void RenderThread::start() {

	if(mAChannls!= -1 && mASampleRate!= -1 ){
		TLOGD("create AudioTrack with channel %d sampleRate %d " , mAChannls,mASampleRate);
		mpTrack = new AudioTrack((uint32_t) mAChannls, (uint32_t) mASampleRate);
	}
	if(mNWin != NULL && mVWidth != -1 && mVHeight != -1 ){
		TLOGD("create SurfaceView with mNWin %p width %d height %d " , mNWin, mVWidth , mVHeight);
		mpView = new SurfaceView((ANativeWindow *)mNWin, mVWidth , mVHeight);
	}

	int ret = ::pthread_create(&mRenderTh, NULL, renderThreadEntry, this);
	if( ret != 0 ){
		mRenderTh = -1 ;
		TLOGE("pthread_create ERROR %d %s " , errno ,strerror(errno));
	}
}

int32_t RenderThread::getCurrent() {
	if(mpTrack != NULL ){
		return (int32_t) (mpTrack->pts() / 1000);
	}else if(mpView != NULL ){
		return (int32_t) (mpView->pts() / 1000);
	}
	TLOGW("getCurrent Not ");
	return -1 ;

}
void RenderThread::pause() {
	//TODO 暂停播放
}

void RenderThread::stop() {
	TLOGD("RenderThread::stop Enter ");
	mStop = true ;
	if( mRenderTh != -1 ){
		{
			AutoMutex l(mQueMtx);
			mAQueCond.signal();
			mVQueCond.signal();
			mSrcCond.signal();
		}
		TLOGD("RenderThread::stop Join Enter ");
		::pthread_join(mRenderTh , NULL );
		TLOGD("RenderThread::stop Join Done ");
		mRenderTh = -1;
	}

	{
		AutoMutex l(mQueMtx);
		TLOGI("pending Audio Render Queue size %lu", mAudRdrQue.size() );
		std::list<sp<Buffer>>::iterator it = mAudRdrQue.begin();
		while(it != mAudRdrQue.end()) {
			mAudRdrQue.erase(it++);
		}

		TLOGI("pending Video Render Queue size %lu", mVidRdrQue.size() );
		it = mVidRdrQue.begin();
		while(it != mVidRdrQue.end()) {
			mVidRdrQue.erase(it++);
		}

		TLOGI("clean up Render Queue Done!");
	}


	if(mpTrack!=NULL){
		delete mpTrack; mpTrack = NULL;
	}

	if(mpView != NULL){
		delete mpView ; mpView = NULL;
	}
	TLOGD("RenderThread::stop Done ");
}


RenderThread::~RenderThread()
{
	if( mpSwsCtx != NULL ) {
		sws_freeContext(mpSwsCtx);
		av_frame_free(&mSrcFrame);
		av_frame_free(&mDstFrame);
	}
	TLOGD("~RenderThread");
}


