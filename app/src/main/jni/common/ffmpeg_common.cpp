/*
 * ffmpeg_common.cpp
 *
 *  Created on: 2016年12月10日
 *      Author: hanlon
 */

#define LOG_TAG "ffmpeg_common"
#include "jni_common.h"
#include "ffmpeg_common.h"
#include <string.h>


void ffmpeg_strerror( int error_num , const char* prefix )
{
	char buf[128];
	av_strerror(error_num, buf, 128);
	ALOGE("%serror %d %s" , ( prefix != NULL ? prefix : "" ) , error_num , buf);
	return ;
}


const char* const pixelFormat2str(AVPixelFormat pixel ){
	switch(pixel){
		case AV_PIX_FMT_YUV420P:
			return "planar YUV 4:2:0";
		case AV_PIX_FMT_BGR24:
			return "packed RGB 8:8:8 BGRBGR...";
		case AV_PIX_FMT_YUV444P:
			return "planar YUV 4:4:4";
		case AV_PIX_FMT_YUYV422:
			return "packed YUV 4:2:2 Y0 Cb Y1 Cr";
		case AV_PIX_FMT_YUV422P:
			return "planar YUV 4:2:2";
		default:
			return "unknown";
	}
}

