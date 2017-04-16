//
// Created by Hanloong Ho on 17-4-12.
//

#ifndef DEMO_FFMPEG_AS_JCLASS_H
#define DEMO_FFMPEG_AS_JCLASS_H


#include <jni.h>

typedef struct {
    jclass 		thizClass;
    jmethodID   postEvent; // post event to Java Layer/Callback Function
} JNIDragonPlayer ;


#endif //DEMO_FFMPEG_AS_JCLASS_H
