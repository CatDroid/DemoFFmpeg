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
	#include "libavutil/error.h"
}
void ffmpeg_strerror( int error_num , const char* prefix = NULL);


#endif /* FFMPEG_COMMON_H_ */
