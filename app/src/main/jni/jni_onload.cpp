#include <jni.h>

#define LOG_TAG "jni_onload"
#include "jni_common.h"

extern "C"{
#include "libavutil/log.h"
};


void av_log_default_callback(void* ptr, int level, const char* fmt, va_list vl)
{
    //FILE *fp = fopen("my_log.txt","a+");
    //if(fp){
    //    vfprintf(fp,fmt,vl);
    //    fflush(fp);
    //    fclose(fp);
    //}

    // AV_LOG_TRACE 可能导致运行非常卡
    //if( level > AV_LOG_DEBUG  ) return ; // 跟多打印
    if( level > AV_LOG_VERBOSE  ) return ;

    int alevel = ANDROID_LOG_VERBOSE;
    if (level <= AV_LOG_ERROR) 		// AV_LOG_PANIC AV_LOG_FATAL
        alevel = ANDROID_LOG_ERROR;
    else if (level <= AV_LOG_WARNING)
        alevel =  ANDROID_LOG_WARN;
    else if (level <= AV_LOG_INFO)
        alevel = ANDROID_LOG_INFO;
    else if (level <= AV_LOG_DEBUG) //AV_LOG_VERBOSE  AV_LOG_DEBUG
        alevel = ANDROID_LOG_DEBUG ;
    else 							// AV_LOG_TRACE
        alevel = ANDROID_LOG_VERBOSE;

    char line[1024];
    static int print_prefix = 1;
    av_log_format_line(ptr, level, fmt, vl, line, sizeof(line), &print_prefix);
    __android_log_print(alevel , "ffmpeg" , "%s", line );

}
/*
 *
[h264 @ 0x7f7f761900] nal_unit_type: 7, nal_ref_idc: 3 <= 67 SPS
[h264 @ 0x7f7f761900] nal_unit_type: 8, nal_ref_idc: 3 <= 68 PPS
[h264 @ 0x7f7f761900] nal_unit_type: 5, nal_ref_idc: 3 <= 65 IDR

[h264 @ 0x7f7f763c00] nal_unit_type: 5, nal_ref_idc: 3
[h264 @ 0x7f7f764100] nal_unit_type: 1, nal_ref_idc: 2 <= 41 P
[h264 @ 0x7f7f762d00] nal_unit_type: 1, nal_ref_idc: 2
[h264 @ 0x7f7f763200] nal_unit_type: 1, nal_ref_idc: 0 <= 01 B
 *
 */


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

    av_log_set_callback(av_log_default_callback);
    //av_log_set_level(AV_LOG_INFO); // 没有作用  不会再回调callback的前 过滤 需要自己过滤 cf. av_log_default_callback @ log.c



	return JNI_VERSION_1_6 ;
}
