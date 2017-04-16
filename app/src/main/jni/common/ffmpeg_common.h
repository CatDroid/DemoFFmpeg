/*
 * ffmpeg_common.h
 *
 *  Created on: 2016年12月10日
 *      Author: hanlon
 */

#ifndef FFMPEG_COMMON_H_
#define FFMPEG_COMMON_H_

#include "string.h"

extern "C" {
	#include "libavutil/frame.h"
	#include "libavutil/imgutils.h"
	#include "libavutil/error.h"
	#include "libavformat/avformat.h"
	#include "libavcodec/avcodec.h"
	#include "libswscale/swscale.h"

}
void ffmpeg_strerror( int error_num , const char* prefix = NULL);

const char* const pixelFormat2str(AVPixelFormat pixel );

#endif /* FFMPEG_COMMON_H_ */
