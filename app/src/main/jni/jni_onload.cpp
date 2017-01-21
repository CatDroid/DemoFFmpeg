#include <jni.h>

#define LOG_TAG "jni_onload"
#include "jni_common.h"


jboolean register_com_tom_ffmpegAPI_MP4player(JNIEnv* env );

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env ;
    if ( vm->GetEnv( (void**) &env, JNI_VERSION_1_6 )  != JNI_OK) {
    	ALOGE("GetEnv Err");
    	return JNI_ERR;
    }

    if( register_com_tom_ffmpegAPI_MP4player(env) != JNI_TRUE){
    	ALOGE("register_com_tom_ffmpegAPI_MP4player Err");
    	return JNI_ERR;
    }

    jni_setJVM(vm);

	return JNI_VERSION_1_6 ;
}
