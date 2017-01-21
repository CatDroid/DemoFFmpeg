/*
 * SurfaceView.h
 *
 *  Created on: 2016年12月17日
 *      Author: hanlon
 */

#ifndef SURFACEVIEW_H_
#define SURFACEVIEW_H_

#include <android/native_window_jni.h> // ANativeWindow

class SurfaceView
{
public:
	SurfaceView(ANativeWindow * window, uint32_t width , uint32_t height );
	~SurfaceView();

	void draw(uint8_t* buffer , uint32_t size , uint32_t width , uint32_t height );
private:
	 ANativeWindow *mpSurface ;
	 uint32_t mWidth ;
	 uint32_t mHeight ;

};

#endif /* SURFACEVIEW_H_ */
