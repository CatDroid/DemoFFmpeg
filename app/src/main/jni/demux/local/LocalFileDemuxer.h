/*
 * LocalFileDemuxer.h
 *
 *  Created on: 2016年12月10日
 *      Author: hanlon
 */

#ifndef LOCALFILEDEMUXER_H_
#define LOCALFILEDEMUXER_H_


#include <pthread.h>
#include <stddef.h>
#include <unistd.h>
#include <string>
#include <list>
#include "IDeMuxerSink.h"
#include "MyPacket.h"
extern "C"{
/*
 * 	编译链接时找不到对应的函数
	undefined reference to `av_register_all()'
	undefined reference to `avformat_open_input(AVFormatContext**, char const*, AVInputFormat*, AVDictionary**)'
	undefined reference to `avformat_find_stream_info(AVFormatContext*, AVDictionary**)'
 */
#include "libavformat/avformat.h"
#include "libavutil/error.h"
}



class LocalFileDemuxer
{
private:
	AVFormatContext * mAvFmtCtx;
	int mVstream ;
	int mAstream ;
	double mVTimebase ;
	double mATimebase ;

	pthread_t mReadThread ;
	std::string     mSPS; // with 00 00 00 01
	std::string     mPPS;
	std::string     mESDS;
	bool			mLoop ;
	bool			mEof ;
private:
	static void* extractThread( void* arg);
	void loop() ;
	void setupVideoSpec(bool is_pps , bool addLeaderCode , unsigned char *data, int size);
	void setupAudioSpec(bool addLeaderCode , unsigned char *data, int size);
public :
	LocalFileDemuxer(const char * file_path );
	virtual ~LocalFileDemuxer();
	virtual AVCodecParameters* getVideoCodecPara();
	virtual AVCodecParameters* getAudioCodecPara();
	double getAudioTimebase(){return mATimebase;};
	double getVideoTimebase(){return mVTimebase;}
	void play();
	void stop();

	DeMuxerSinkBase* mAudioSinker ;
	DeMuxerSinkBase* mVideoSinker ;
	void setAudioSinker(DeMuxerSinkBase* audioSinker);
	void setVideoSinker(DeMuxerSinkBase* videoSinker);

	sp<PacketManager> mPm ;

};



#endif /* LOCALFILEDEMUXER_H_ */
