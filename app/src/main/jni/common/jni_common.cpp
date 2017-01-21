/*
 * jni_common.cpp
 *
 *  Created on: 2016年12月10日
 *      Author: hanlon
 */

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#define LOG_TAG "jni_common"
#include "jni_common.h"


JavaVM *g_JVM;

void jni_setJVM(JavaVM * jvm )
{
	g_JVM = jvm ;
}

int jniThrowException(JNIEnv* env, const char* className, const char* msg) {
    if (env->ExceptionCheck()) {
    	ALOGE("jniThrowException Exception in Exception : Clear and Throw New Excepiion");
    	env->ExceptionClear();
    }

     jclass expclass =  env->FindClass(className );
    if (expclass == NULL) {
        ALOGE("Unable to find exception class %s", className);
        return -1;
    }

    if ( env ->ThrowNew( expclass , msg) != JNI_OK) {
        ALOGE("Failed throwing '%s' '%s'", className, msg);
        return -1;
    }

    return 0;
}
int jniThrowNullPointerException(JNIEnv* env, const char* msg) {
    return jniThrowException(env, "java/lang/NullPointerException", msg);
}

int jniThrowRuntimeException(JNIEnv* env, const char* msg) {
    return jniThrowException(env, "java/lang/RuntimeException", msg);
}

int jniThrowIOException(JNIEnv* env, int errnum) {
    const char* message = strerror(errnum);
    return jniThrowException(env, "java/io/IOException", message);
}


jboolean checkCallbackThread(JavaVM* vm , JNIEnv* isTargetEnv) {
	JNIEnv* currentThreadEnv = NULL;
    if ( vm->GetEnv( (void**) &currentThreadEnv, JNI_VERSION_1_6) != JNI_OK) {
    	return JNI_FALSE;
    }

    if (isTargetEnv != currentThreadEnv || isTargetEnv == NULL) {
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

void AttachThread2JVM( JavaVM* vm , JNIEnv** ppEnv ,/* out */
									const char* const threadName)
{
	JavaVMAttachArgs args;
	args.version = JNI_VERSION_1_6;
	args.name = threadName ;
	args.group = NULL;
	if ( vm->AttachCurrentThread(ppEnv, &args) != JNI_OK){
		ALOGE("Fatal Exception: AttachCurrentThread Fail ");
	}else{
		ALOGI("attach %s thread to JVM done" , threadName);
	}
}

void DetachThread2JVM(JavaVM* vm  , JNIEnv*pEnv /* in */ )
{
	if (!checkCallbackThread(vm, pEnv)) {
		return;
    }
	vm->DetachCurrentThread();

}

int64_t system_nanotime()
{
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * 1000000000LL + now.tv_nsec;
}



