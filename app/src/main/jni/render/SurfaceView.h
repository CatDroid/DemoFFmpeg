/*
 * SurfaceView.h
 *
 *  Created on: 2016年12月17日
 *      Author: hanlon
 */

#ifndef SURFACEVIEW_H_
#define SURFACEVIEW_H_

#include <android/native_window_jni.h> // ANativeWindow
#include <list>
#include <Log.h>
#include "Buffer.h"
#include "Condition.h"
#include "Mutex.h"

class SurfaceView
{
private:
	ANativeWindow *mpSurface ;
	uint32_t mWidth ;
	uint32_t mHeight ;
	std::list<sp<Buffer>> mDisBuf;
	Mutex mDisMux;
	Condition mDisCon;
	pthread_t mDisThID ;
	bool mStop ;
public:
	SurfaceView(ANativeWindow * window, uint32_t width , uint32_t height );
	~SurfaceView();
	void draw(sp<Buffer>);

private:
	void loop();
	static void* dispThreadEntry(void *arg);
	void draw(uint8_t* buffer , uint32_t size , uint32_t width , uint32_t height );

private:
	CLASS_LOG_DECLARE(SurfaceView);
};

#endif /* SURFACEVIEW_H_ */
