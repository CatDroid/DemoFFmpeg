/*
 * IDemuxerSink.h
 *
 *  Created on: 2016年12月11日
 *      Author: hanlon
 */

#ifndef IDEMUXERSINK_H_
#define IDEMUXERSINK_H_

#include <string.h>
#include "MyPacket.h"

extern "C"{
#include "libavformat/avformat.h"
#include "libavutil/error.h"
}


class IDeMuxerSink
{
public:
	virtual bool put ( sp<MyPacket> packet) = 0 ; // mayblock
	virtual ~IDeMuxerSink() {} ;
};

class DeMuxerSinkBase : public IDeMuxerSink {
public:
	virtual bool put( sp<MyPacket> ){ /* release buffer auto*/ }
	virtual ~DeMuxerSinkBase() { }
};

#endif /* IDEMUXERSINK_H_ */
