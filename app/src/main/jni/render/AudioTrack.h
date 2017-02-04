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

	friend class RenderThread;
	bool isStoped() { return mStoped ; }
public:
	AudioTrack(uint32_t channel /*1 or 2 */, uint32_t sample_rate /*unit:Hz*/);
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
	volatile  bool mStoped;

	int64_t pts() const {return mCurrentPts;}

private:
	int64_t mCurrentPts ; // ms

	sp<Buffer> mLastQueuedBuffer ;

	// Only for debug +++
	sp<SaveFile> mSaveFile;

	// Only for debug ---

};



#endif /* AUDIOTRACK_H_ */
