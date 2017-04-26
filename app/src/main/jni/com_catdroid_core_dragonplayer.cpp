/*
 * com_dragon_core_dragonplayer.cpp
 *
 *  Created on: 2016年10月17日
 *      Author: Hanloong Ho
 */

#define LOG_TAG "jni_player"

#include <JClass.h>
#include "Player.h"
#include "jni_common.h"
#include "JClass.h"

JNIDragonPlayer gJNIDragonPlayer ;

struct JNIPlayer{
	Player* thizPlayer ;
};


static long JNICALL native_setup(JNIEnv *env, jobject thiz, jobject player_this_ref )
{
	JNIPlayer* jplayer = new JNIPlayer();
	jplayer->thizPlayer = new Player(env , player_this_ref);
	LOGT(LOG_TAG , "native_setup %p" , (void*)jplayer );
	return (long) jplayer;
}

static jboolean JNICALL native_setDataSource(JNIEnv *env, jobject thiz, jlong ctx, jstring path)
{
	LOGT(LOG_TAG , "native_setDataSource %p" , (void*)ctx );
	const char* file = NULL;
	JNI_GET_UTF_CHAR( file , path);
	JNIPlayer* jplayer = (JNIPlayer*)ctx ;
	bool result = jplayer->thizPlayer->setDataSource(file);
	JNI_RELEASE_STR_STR(file , path);
	return result?(jboolean)JNI_TRUE:(jboolean)JNI_FALSE;
}
static void JNICALL native_prepareAsync(JNIEnv *env, jobject thiz, jlong ctx)
{
	LOGT(LOG_TAG , "native_prepareAsync %p" , (void*)ctx );
	JNIPlayer* jplayer = (JNIPlayer*)ctx ;
	jplayer->thizPlayer->prepare();
}


static jboolean JNICALL  native_setDisplay(JNIEnv *env, jobject thiz, jlong ctx, jobject jSurface )
{
	LOGT(LOG_TAG , "native_prepareAsync %p" , (void*)ctx );
	JNIPlayer* jplayer = (JNIPlayer*)ctx ;
	jplayer->thizPlayer->setView(env,jSurface);
	return JNI_TRUE ;
}

static void JNICALL native_play(JNIEnv *env, jobject thiz, jlong ctx )
{
	LOGT(LOG_TAG , "native_play %p" , (void*)ctx );
	JNIPlayer* jplayer = (JNIPlayer*)ctx ;
	jplayer->thizPlayer->play();
}
static void JNICALL native_pause(JNIEnv *env, jobject thiz, jlong ctx)
{
	LOGT(LOG_TAG , "native_pause %p" , (void*)ctx );
}

static void JNICALL native_seekTo(JNIEnv *env, jobject thiz, jlong ctx , jint msec)
{
	LOGT(LOG_TAG , "native_seekTo %p" , (void*)ctx );
}

static void JNICALL native_stop(JNIEnv *env, jobject thiz, jlong ctx)
{
	LOGT(LOG_TAG , "native_stop %p" , (void*)ctx );
	JNIPlayer* jplayer = (JNIPlayer*)ctx ;
	jplayer->thizPlayer->stop();
}

static void JNICALL native_free(JNIEnv *env, jobject thiz, jlong ctx )
{
	LOGT(LOG_TAG , "native_free %p" , (void*)ctx );
	JNIPlayer* jplayer = (JNIPlayer*)ctx ;
	delete jplayer->thizPlayer;
	delete jplayer ;
}


static jboolean JNICALL  native_setVolume(JNIEnv *env, jobject thiz, jlong ctx , jfloat left , jfloat right )
{
	return JNI_FALSE ;
}

static jboolean JNICALL  native_setMute(JNIEnv *env, jobject thiz, jlong ctx , jboolean on)
{
	return JNI_FALSE ;
}

static jint JNICALL native_getDuration(JNIEnv *env, jobject thiz, jlong ctx )
{
	LOGT(LOG_TAG , "native_getDuration %p" , (void*)ctx );
	JNIPlayer* jplayer = (JNIPlayer*)ctx ;
	return jplayer->thizPlayer->getDuration() ;
}
static jint JNICALL native_getCurrentPosition(JNIEnv *env, jobject thiz, jlong ctx )
{
	LOGT(LOG_TAG , "native_getCurrentPosition %p" , (void*)ctx );
	JNIPlayer* jplayer = (JNIPlayer*)ctx ;
	return jplayer->thizPlayer->getCurrent();
}

static jint JNICALL native_getAudioChannel(JNIEnv *env, jobject thiz, jlong ctx )
{
	return 0 ;
}
static jint JNICALL native_getAudioDepth(JNIEnv *env, jobject thiz, jlong ctx )
{
	return 0 ;
}
static jint JNICALL native_getAudioSampleRate(JNIEnv *env, jobject thiz, jlong ctx )
{
	return 0 ;
}
static jint JNICALL native_getVideoWidth(JNIEnv *env, jobject thiz, jlong ctx )
{
	LOGT(LOG_TAG , "native_getVideoWidth %p" , (void*)ctx );
	JNIPlayer* jplayer = (JNIPlayer*)ctx ;
	return jplayer->thizPlayer->getWidth();
}
static jint JNICALL native_getVideoHeight(JNIEnv *env, jobject thiz, jlong ctx )
{
	LOGT(LOG_TAG , "native_getVideoHeight %p" , (void*)ctx );
	JNIPlayer* jplayer = (JNIPlayer*)ctx ;
	return jplayer->thizPlayer->getHeigth();
}

static void JNICALL native_subscribeData(JNIEnv *env, jobject thiz, jlong ctx , jint data_type , jboolean isOn)
{

}

#define  DRAGON_PLAYER_JAVA_CLASS_PATH "com/catdroid/core/DragonPlayer"
jboolean register_com_catdroid_core_dragonplayer(JNIEnv *env)
{
	jclass clazz;
    clazz = env->FindClass(DRAGON_PLAYER_JAVA_CLASS_PATH );
    if (clazz == NULL) {
		ALOGE("%s:Class Not Found" , DRAGON_PLAYER_JAVA_CLASS_PATH );
		return JNI_ERR ;
    }

	JNINativeMethod method_table[] = {
		{ "native_setup", 				"(Ljava/lang/Object;)J", (void*)native_setup },

		{ "native_setDataSource",		"(JLjava/lang/String;)Z",		(void*)native_setDataSource },
		{ "native_prepareAsync",		"(J)V", 						(void*)native_prepareAsync },
		{ "native_free",				"(J)V", 						(void*)native_free },

		{ "native_setDisplay",			"(JLandroid/view/Surface;)Z",	(void*)native_setDisplay },
		{ "native_play",				"(J)V", 						(void*)native_play },
		{ "native_pause",				"(J)V", 						(void*)native_pause },
		{ "native_seekTo",				"(JI)V", 						(void*)native_seekTo },
		{ "native_stop", 				"(J)V", 						(void*)native_stop },

		{ "native_setVolume", 			"(JFF)Z", 						(void*)native_setVolume },
		{ "native_setMute", 			"(JZ)Z", 						(void*)native_setMute },
		{ "native_getAudioChannel",		"(J)I", 						(void*)native_getAudioChannel },
		{ "native_getAudioDepth",		"(J)I", 						(void*)native_getAudioDepth },
		{ "native_getAudioSampleRate",	"(J)I", 						(void*)native_getAudioSampleRate },

		{ "native_getDuration", 		"(J)I", 						(void*)native_getDuration },
		{ "native_getCurrentPosition",	"(J)I", 						(void*)native_getCurrentPosition },
		{ "native_getVideoWidth",		"(J)I", 						(void*)native_getVideoWidth },
		{ "native_getVideoHeight",		"(J)I", 						(void*)native_getVideoHeight },

		{ "native_subscribeData", 			"(JIZ)V",						(void*)native_subscribeData },
	};
	jniRegisterNativeMethods( env, DRAGON_PLAYER_JAVA_CLASS_PATH ,  method_table, NELEM(method_table)) ;

//    field fields_to_find[] = {
//        { DRAGON_PLAYER_JAVA_CLASS_PATH , "mNativeContext",  "J", &g_java_field.context },
//    };
//    find_fields( env , fields_to_find, NELEM(fields_to_find) );

	gJNIDragonPlayer.thizClass = (jclass)env->NewGlobalRef(clazz);
	gJNIDragonPlayer.postEvent = env->GetStaticMethodID( clazz, "postEventFromNative", "(Ljava/lang/Object;IIILjava/lang/Object;)V");

	return JNI_TRUE;

}

