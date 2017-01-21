/*
 * IDemuxerSink.h
 *
 *  Created on: 2016年12月11日
 *      Author: hanlon
 */

#ifndef IDEMUXERSINK_H_
#define IDEMUXERSINK_H_

#include <string.h>
#include <Buffer.h>

extern "C"{
#include "libavformat/avformat.h"
#include "libavutil/error.h"
}



class IMuxerSink
{
public:
	virtual bool put ( sp<Buffer> packet) = 0 ; // mayblock
	virtual ~IMuxerSink() {} ;
};

class DeMuxerSinkBase : public IMuxerSink {
public:
	virtual bool put( sp<Buffer> ){ /* release buffer auto*/ }
	virtual ~DeMuxerSinkBase() { }
};

#endif /* IDEMUXERSINK_H_ */
