/*
 * SurfaceView.cpp
 *
 *  Created on: 2016年12月17日
 *      Author: hanlon
 */

#define LOG_TAG "SurfaceView"
#include "jni_common.h"

#include "SurfaceView.h"
#include <assert.h>


SurfaceView::SurfaceView(ANativeWindow * window , uint32_t width , uint32_t height ):
				mpSurface(window),mWidth(width),mHeight(height)
{
	ALOGD("create view [%d %d] " , mWidth , mHeight);
	ANativeWindow_setBuffersGeometry(mpSurface, mWidth, mHeight, WINDOW_FORMAT_RGBX_8888);
}

SurfaceView::~SurfaceView()
{

}

void SurfaceView::draw(uint8_t* pBuf , uint32_t size , uint32_t width  , uint32_t height )
{
	assert(buffer != NULL);

	// 	ANativeWindow_getWidth
	// 	ANativeWindow_getHeight  // 获取当前window的宽高
	if( mWidth!= width || mHeight != height ){
		ALOGD("Format Change [%d %d] -> [%d %d]!" , mWidth , mHeight , width , height );
		mWidth = width ;
		mHeight = height ;
		ANativeWindow_setBuffersGeometry(mpSurface, mWidth,  mHeight, WINDOW_FORMAT_RGBX_8888);
	}

	ANativeWindow_Buffer wbuffer;
	if( ANativeWindow_lock(mpSurface, &wbuffer, 0) != 0 ){
		ALOGE("lock window fail !");
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






