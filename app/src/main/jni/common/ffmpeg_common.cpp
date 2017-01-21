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
