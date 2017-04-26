/*
 * SurfaceView.cpp
 *
 *  Created on: 2016年12月17日
 *      Author: hanlon
 */

#include "SurfaceView.h"

#include <assert.h>
#include <sys/prctl.h>
#include <inttypes.h>

#include "project_utils.h"

CLASS_LOG_IMPLEMENT(SurfaceView,"SurfaceView");

//#define TRACE_SurfaceView 1
#define MAX_DELAY_DISPLAY_FRAME 2

SurfaceView::SurfaceView(ANativeWindow * window , uint32_t width , uint32_t height ):
				mpSurface(window),mWidth(width),mHeight(height),mDisThID(-1),mStop(false),mCurrentPts(0)
{
	TLOGT("SurfaceView");
	TLOGD("create view [%d %d] " , mWidth , mHeight);
	ANativeWindow_setBuffersGeometry(mpSurface, mWidth, mHeight, WINDOW_FORMAT_RGBX_8888);

	int ret = 0 ;
	ret = ::pthread_create(&mDisThID, NULL, SurfaceView::dispThreadEntry, (void *) this);
	if(ret < 0){
		TLOGE("Create SurfaceView Thread ERROR %d %d %s" , ret ,errno, strerror(errno));
		assert(ret >= 0 );
	}

}


SurfaceView::~SurfaceView()
{
	mStop = true ;
	if(mDisThID != -1) {
		{
			AutoMutex l(mDisMux);
			mDisCon.signal();
		}
		pthread_join(mDisThID, NULL);
		{
			AutoMutex l(mDisMux);
			std::list<sp<Buffer>>::iterator it = mDisBuf.begin();
			while(it != mDisBuf.end()) {
				mDisBuf.erase(it++);
			}
			TLOGI("SurfaceView clean up Buffer Done!");
		}
		TLOGD("SurfaceView threadTh stop done");
	}else{
		TLOGD("SurfaceView threadTh not start yet ");
	}

	ANativeWindow_release(mpSurface);
	TLOGT("~SurfaceView");
}

void* SurfaceView::dispThreadEntry(void *arg)
{
	prctl(PR_SET_NAME,"SurfaceViewTh");
	SurfaceView* sv = (SurfaceView*)arg;
	sv->loop();
	return NULL;
}


void SurfaceView::draw(sp<Buffer> buf){

	if(!mStop){
		{
			AutoMutex _l(mDisMux);
			if(mDisBuf.size() > MAX_DELAY_DISPLAY_FRAME){
									// 如果经常draw超过16ms就可能出现mDisBuf越来越多 故这里需要丢帧
				mDisBuf.pop_front();// 这样mRgbBm的 Buffer数目 最大就在 30(RenderThread.mVidRdrQue)+2(MAX_DELAY_DISPLAY_FRAME)
				TLOGW("too much to display, drop frame ");
			}
			mDisBuf.push_back(buf);
			mDisCon.signal();
#if TRACE_SurfaceView == 1
			TLOGT("Display Buffers %lu ", mDisBuf.size() );
#endif
		}
	}else{
		TLOGT("SurfaceView is Stoped");
	}
}

void SurfaceView::loop()
{
	TLOGW("SurfaceView loop entry");

	// Process.THREAD_PRIORITY_DISPLAY = -4
	// SurfaceView线程可能优先级底 不能及时运行 导致Buffer不能及时归还
	// AudioTrack 是基于OpenSL回调线程优先级是 -16
	//nice(-4);

	while(!mStop){
		sp<Buffer> next = NULL ;
		{
			AutoMutex _l(mDisMux);
			if(mDisBuf.empty()){
				if(mStop) break;
				mDisCon.wait(mDisMux);
				continue;
			}else{
				next = mDisBuf.front();
				mDisBuf.pop_front();
			}
		}

		if(next.get()==NULL) continue;
#if TRACE_SurfaceView == 1
		int64_t before = getCurTimeUs()  ;
		TLOGT("actual draw buffer %p data %p", next.get() , (void*)next->data() );
#endif
		draw(next->data() , (uint32_t) next->size(), (uint32_t) next->width(), (uint32_t) next->height());
		mCurrentPts = next->pts();
#if TRACE_SurfaceView == 1
		int64_t cost = (getCurTimeUs()-before);
		if( cost > 16000 ) {// 16ms 这里耗时可能大于16ms
			TLOGW("actual draw too long cost %" PRId64 " us" , cost );
		}else{
			TLOGT("actual draw cost %" PRId64 " us" , cost );
		}
#endif
	}

	TLOGW("SurfaceView loop exit");
}



void SurfaceView::draw(uint8_t* pBuf , uint32_t size , uint32_t width  , uint32_t height )
{
	assert(pBuf != NULL);

	// 	ANativeWindow_getWidth
	// 	ANativeWindow_getHeight  // 获取当前window的宽高
	if( mWidth!= width || mHeight != height ){
		TLOGD("Format Change [%d %d] -> [%d %d]!" , mWidth , mHeight , width , height );
		mWidth = width ;
		mHeight = height ;
		ANativeWindow_setBuffersGeometry(mpSurface, mWidth,  mHeight, WINDOW_FORMAT_RGBX_8888);
	}

	ANativeWindow_Buffer wbuffer;
	if( ANativeWindow_lock(mpSurface, &wbuffer, 0) != 0 ){
		TLOGE("lock window fail !");
		assert(false);
	}else{
		assert( ((wbuffer.width == mWidth) && (wbuffer.height == mHeight)) );
		if( wbuffer.stride == wbuffer.width ){
			memcpy(wbuffer.bits, pBuf, width  * height * 4 );
		}else{
			for(int line = 0; line < height; ++line){
				memcpy((uint8_t*)wbuffer.bits + wbuffer.stride * line * 4,
						pBuf + width * line * 4,
						width * 4);
			}
		}
		ANativeWindow_unlockAndPost(mpSurface);
	}
}






