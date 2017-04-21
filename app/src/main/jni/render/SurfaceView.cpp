/*
 * SurfaceView.cpp
 *
 *  Created on: 2016年12月17日
 *      Author: hanlon
 */

#define LOG_TAG "SurfaceView"
#include "SurfaceView.h"
#include <assert.h>
#include <sys/prctl.h>

CLASS_LOG_IMPLEMENT(SurfaceView,"SurfaceView");

SurfaceView::SurfaceView(ANativeWindow * window , uint32_t width , uint32_t height ):
				mpSurface(window),mWidth(width),mHeight(height),mDisThID(-1),mStop(false)
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
			mDisBuf.push_back(buf);
			mDisCon.signal();
		}
	}else{
		TLOGT("SurfaceView is Stoped");
	}
}

void SurfaceView::loop()
{
	TLOGW("SurfaceView loop entry");

	sp<Buffer> next = NULL ;
	while(!mStop){
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

		draw(next->data() , (uint32_t) next->size(), (uint32_t) next->width(), (uint32_t) next->height());
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






