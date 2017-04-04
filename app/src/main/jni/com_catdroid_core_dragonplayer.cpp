/*
 * com_dragon_core_dragonplayer.cpp
 *
 *  Created on: 2016年10月17日
 *      Author: Hanloong Ho
 */


#define LOG_TAG "jni_mp4player"
#include "jni_common.h"
#include "LocalFileDemuxer.h"
#include "H264SWDecoder.h"
#include "SurfaceView.h"
#include "AudioTrack.h"
#include "AACSWDecoder.h"

static struct g_java_fields {
    jfieldID    context; 	// native object pointer
    jmethodID   post_event; // post event to Java Layer/Callback Function
} g_java_field;

struct Mp4player{
	LocalFileDemuxer*  mpDeMuxer ;
	H264SWDecoder*  mpVDecoder ;
	AACSWDecoder* mpADecoder ;
	SurfaceView* mpView ;
	AudioTrack* mpTrack ;
	RenderThread* mpRender ;
};

static void JNICALL native_playmp4 (JNIEnv* env , jobject mp4player , jlong ctx, jstring file_obj , jobject surface , jobject at )
{
	const char* file = NULL;
	JNI_GET_UTF_CHAR( file , file_obj);


	Mp4player* player = new Mp4player();
	player->mpDeMuxer = new LocalFileDemuxer(file);

	AVCodecParameters* vpara = player->mpDeMuxer->getVideoCodecPara();
	if( vpara != NULL){
		switch(vpara->codec_id){
		case AV_CODEC_ID_H264:
			player->mpVDecoder  = new H264SWDecoder(vpara ,  player->mpDeMuxer->getVideoTimebase() );
			break;
		default:
			ALOGE("don NOT support %d " , vpara->codec_id );
			assert(vpara->codec_id == AV_CODEC_ID_H264);
			break;
		}
	}

	AVCodecParameters* apara = player->mpDeMuxer->getAudioCodecPara();
	if( apara != NULL){
		switch(apara->codec_id){
		case AV_CODEC_ID_AAC:
			player->mpADecoder = new AACSWDecoder(apara ,  player->mpDeMuxer->getAudioTimebase()  );
			break;
		default:
			ALOGE("don NOT support %d " , apara->codec_id );
			assert(apara->codec_id == AV_CODEC_ID_AAC);
			break;
		}
	}

	// release at SurfaceView
	player->mpView = new SurfaceView(ANativeWindow_fromSurface(env,surface), vpara->width, vpara->height );
	player->mpTrack = new AudioTrack(apara->channels, apara->sample_rate);
	player->mpRender = new RenderThread(player->mpView, player->mpTrack);


	player->mpDeMuxer->setAudioSinker(player->mpADecoder);
	player->mpDeMuxer->setVideoSinker(player->mpVDecoder);
	player->mpVDecoder->start(player->mpRender);
	player->mpADecoder->start(player->mpRender);
	player->mpDeMuxer->play();


	env->SetLongField(mp4player ,g_java_field.context, (long)player);
	JNI_RELEASE_STR_STR(file , file_obj);

}


//static void JNICALL native_stop (JNIEnv* jenv , jobject mp4player, jlong ctx)
//{
//	Mp4player* player =  (Mp4player*)ctx ;
//	jenv->SetLongField(mp4player ,g_java_field.context, 0);
//	player->mpDeMuxer->stop();
//	player->mpVDecoder->stop();
//	player->mpADecoder->stop();
//	delete player->mpVDecoder;
//	delete player->mpADecoder;
//	delete player->mpDeMuxer;
//	delete player->mpRender;
//	delete player->mpTrack;
//	delete player->mpView;
//}


static long JNICALL native_setup(JNIEnv *env, jobject thiz, jobject player_this_ref )
{

}

static jboolean JNICALL native_setDataSource(JNIEnv *env, jobject thiz, jlong ctx, jstring path)
{

}
static void JNICALL native_prepareAsync(JNIEnv *env, jobject thiz, jlong ctx)
{

}

static void JNICALL native_free(JNIEnv *env, jobject thiz, jlong ctx )
{

}

static jboolean JNICALL  native_setDisplay(JNIEnv *env, jobject thiz, jlong player_ctx, jobject jSurface )
{

}

static void JNICALL native_play(JNIEnv *env, jobject thiz, jlong ctx )
{

}
static void JNICALL native_pause(JNIEnv *env, jobject thiz, jlong ctx)
{

}

static void JNICALL native_seekTo(JNIEnv *env, jobject thiz, jlong ctx , jint msec)
{

}

static void JNICALL native_stop(JNIEnv *env, jobject thiz, jlong ctx)
{

}

static jboolean JNICALL  native_setVolume(JNIEnv *env, jobject thiz, jlong ctx , jfloat left , jfloat right )
{

}

static jboolean JNICALL  native_setMute(JNIEnv *env, jobject thiz, jlong ctx , jboolean on)
{

}

static jint JNICALL native_getDuration(JNIEnv *env, jobject thiz, jlong ctx )
{

}
static jint JNICALL native_getCurrentPosition(JNIEnv *env, jobject thiz, jlong ctx )
{

}

static jint JNICALL native_getAudioChannel(JNIEnv *env, jobject thiz, jlong ctx )
{

}
static jint JNICALL native_getAudioDepth(JNIEnv *env, jobject thiz, jlong ctx )
{

}
static jint JNICALL native_getAudioSampleRate(JNIEnv *env, jobject thiz, jlong ctx )
{

}
static jint JNICALL native_getVideoWidth(JNIEnv *env, jobject thiz, jlong ctx )
{

}
static jint JNICALL native_getVideoHeight(JNIEnv *env, jobject thiz, jlong ctx )
{

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

		{ "native_setData", 			"(JIZ)V",						(void*)native_subscribeData },
	};
	jniRegisterNativeMethods( env, DRAGON_PLAYER_JAVA_CLASS_PATH ,  method_table, NELEM(method_table)) ;

//    field fields_to_find[] = {
//        { DRAGON_PLAYER_JAVA_CLASS_PATH , "mNativeContext",  "J", &g_java_field.context },
//    };
//    find_fields( env , fields_to_find, NELEM(fields_to_find) );

    return JNI_TRUE;

}

