/*
 * AudioTrack.h
 *
 *  Created on: 2016年12月17日
 *      Author: hanlon
 */

#ifndef AUDIOTRACK_H_
#define AUDIOTRACK_H_

// for native audio
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <Buffer.h>
#include <Condition.h>
#include <list>
#include "SaveFile.h"


class AudioTrack
{
private:
	// engine interfaces
	SLObjectItf mEngineObject;
	SLEngineItf mIEngineEngine;
	// output mix interfaces
	SLObjectItf mOutputMixObject;

	// buffer queue player interfaces
	SLObjectItf mPlayerObject = NULL;
	SLPlayItf mIPlayerPlay;
	SLAndroidSimpleBufferQueueItf mIPlayerBufferQueue;
	//SLMuteSoloItf mIPlayerMuteSolo;
	SLVolumeItf mIPlayerVolume;

	static void playerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);
	void playerCallback(SLAndroidSimpleBufferQueueItf bq );
public:
	AudioTrack();
	~AudioTrack();
	bool setVolume( float volume);
	bool setMute( bool mute );
	bool write( sp<Buffer> );
	Condition* mBufCon;
	Condition* mBufFullCon;
	Mutex* mBufMux ;
	std::list<sp<Buffer>> mBuf;
	bool mStop;
	bool mStarted;

	double pts() const {return mCurrentPts;}
private:
	double mCurrentPts ;

	sp<SaveFile> mSaveFile;

};



#endif /* AUDIOTRACK_H_ */
