/*
 * AudioTrack.cpp
 *
 *  Created on: 2016年12月17日
 *      Author: hanlon
 */

#define LOG_TAG "AudioTrack"

#include "project_utils.h"
#include "jni_common.h"

#include "AudioTrack.h"

//#define SAVE_DECODE_TO_FILE

#define AUDIO_RENDER_BUFFER_SIZE 10
AudioTrack::AudioTrack(uint32_t channel, uint32_t sample_rate) : mStop(false), mStarted(false),mStoped(false)
{
	SLresult result;


	mBufCon = new Condition();
	mBufMux = new Mutex();

	// create engine
	ALOGD( "创建引擎Engine Object");
	result = slCreateEngine(&mEngineObject, 0, NULL, 0, NULL, NULL);
	assert(SL_RESULT_SUCCESS == result);


	SLuint32 num = 0;
	result = slQueryNumSupportedEngineInterfaces(&num);
	if( result != SL_RESULT_SUCCESS) {
		ALOGE("slQueryNumSupportedEngineInterfaces fail ");
	} else {
		ALOGD("OpenSL 引擎对象支持的接口 包含必须的 和 可选的 num = %d", num );
	}

	for( SLuint32 i = 0; i < num; i ++) {
		SLInterfaceID local_InterfaceID;
		result = slQuerySupportedEngineInterfaces(i,&local_InterfaceID);
		ALOGD("supported Engine Interface  timestamp = %08x %04x %04x ; clock_seq = %04x ; node = %08x  " ,
				local_InterfaceID->time_low ,
				local_InterfaceID->time_mid ,
				local_InterfaceID->time_hi_and_version ,
				local_InterfaceID->clock_seq ,
				local_InterfaceID->node[0] );
	}

	// realize the engine
	// SL_BOOLEAN_FALSE 代表同步   异步???
	result = (*mEngineObject)->Realize(mEngineObject, SL_BOOLEAN_FALSE);
	assert(SL_RESULT_SUCCESS == result);

	ALOGD( "SLObjectItf --> SLEngineItf: 获取引擎对象Engine Object的接口  后面其他Object都要用引擎对象的接口(而非引擎对象本身)来创建");
	result = (*mEngineObject)->GetInterface(mEngineObject, SL_IID_ENGINE, &mIEngineEngine);
	assert(SL_RESULT_SUCCESS == result);


	const SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
	const SLboolean req[1] = {SL_BOOLEAN_FALSE};
	result = (*mIEngineEngine)->CreateOutputMix(mIEngineEngine, &mOutputMixObject, 1, ids, req);
	assert(SL_RESULT_SUCCESS == result);
	ALOGD("通过 引擎对象的接口  创建  输出混音对象 不需要 环境混音接口 ");


	result = (*mOutputMixObject)->Realize(mOutputMixObject, SL_BOOLEAN_FALSE);

	if(SL_RESULT_SUCCESS != result  ){
		ALOGE("Realize OutputMixObject ERROR %d  " ,result );
		assert(SL_RESULT_SUCCESS == result);
	}


	// configure audio source
	SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
			SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
			2 };

	// TODO 应该根据解码的参数 实例化
	SLDataFormat_PCM format_pcm = {
		SL_DATAFORMAT_PCM,
		2, 							// channel
		sample_rate*1000,    		// sample rate in mili second , SL_SAMPLINGRATE_44_1
		SL_PCMSAMPLEFORMAT_FIXED_16,// bit per sample
		SL_PCMSAMPLEFORMAT_FIXED_16 ,
		SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, // SL_SPEAKER_FRONT_CENTER 是 单通道
		SL_BYTEORDER_LITTLEENDIAN};


	if (channel == 2){
		format_pcm.numChannels = 2 ;
		format_pcm.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
	} else if(channel == 1 ) {
		format_pcm.numChannels = 1 ;
		format_pcm.channelMask = SL_SPEAKER_FRONT_CENTER;
	} else {
		ALOGE("Audio Channel should be 1 or 2 ");
		format_pcm.numChannels = 2 ;
		format_pcm.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
	}


	SLDataSource audioSrc = {&loc_bufq, &format_pcm};

	SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, mOutputMixObject};
	SLDataSink audioSnk = {&loc_outmix, NULL};


	const SLInterfaceID ids_player[2] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
	const SLboolean req_player[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

	result = (*mIEngineEngine)->CreateAudioPlayer(
								mIEngineEngine,
								&mPlayerObject,
								&audioSrc,
								&audioSnk,
								2, // numInterfaces
								ids_player,
								req_player);

	assert(SL_RESULT_SUCCESS == result);


	// realize the player
	result = (*mPlayerObject)->Realize(mPlayerObject, SL_BOOLEAN_FALSE);
	assert(SL_RESULT_SUCCESS == result);


	// get the play interface
	result = (*mPlayerObject)->GetInterface(mPlayerObject, SL_IID_PLAY, &mIPlayerPlay);
	assert(SL_RESULT_SUCCESS == result);


    // get the mute/solo interface
    //result = (*mPlayerObject)->GetInterface(mPlayerObject, SL_IID_MUTESOLO, &mIPlayerMuteSolo);
    //assert(SL_RESULT_SUCCESS == result);


	// get the buffer queue interface
	result = (*mPlayerObject)->GetInterface(mPlayerObject, SL_IID_BUFFERQUEUE, &mIPlayerBufferQueue);
	assert(SL_RESULT_SUCCESS == result);


	// get the volume interface
	result = (*mPlayerObject)->GetInterface(mPlayerObject, SL_IID_VOLUME, &mIPlayerVolume);
	assert(SL_RESULT_SUCCESS == result);


	// register callback on the buffer queue 注册函数 让系统回调填充buffer
	result = (*mIPlayerBufferQueue)->RegisterCallback(mIPlayerBufferQueue, playerCallback, this);
	assert(SL_RESULT_SUCCESS == result);


	// set the player's state to playing
	result = (*mIPlayerPlay)->SetPlayState(mIPlayerPlay, SL_PLAYSTATE_PLAYING);
	assert(SL_RESULT_SUCCESS == result);

#ifdef SAVE_DECODE_TO_FILE
	mSaveFile = new SaveFile("/mnt/sdcard/render.pcm");
#endif
}


// this callback handler is called every time a buffer finishes playing
void AudioTrack::playerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
//	SLresult result;
	AudioTrack*  at = (AudioTrack*)context ;
	at->playerCallback(bq);
}

void AudioTrack::playerCallback(SLAndroidSimpleBufferQueueItf bq )
{
	sp<Buffer> playbuf = NULL;
	while(!mStop ){
		AutoMutex l(mBufMux);
		if(mBuf.empty()){
			ALOGD("playerCallback:wait ");
			if(mStop) continue ;
			mBufCon->wait(mBufMux);
			continue ;
		}else{
			playbuf = mBuf.front();
			mBuf.pop_front();
			break;
		}
	}


	// 退出检查
	if( mStop ){
		ALOGD("playerCallback:stop  because mStop == true ");
		mStoped = true ;
		return ;
	}
	if( playbuf.get() == NULL ) {
		ALOGW("playerCallback:End of File because playbuf == NULL ");
		mStoped = true ;
		mLastQueuedBuffer = NULL;
		return ;
	}

	SLAndroidSimpleBufferQueueState state ;
	(*mIPlayerBufferQueue)->GetState(mIPlayerBufferQueue , &state);

	ALOGD("play %p Enqueue %d state count %d index %d " , playbuf.get() ,playbuf->size() , state.count , state.index );
	ALOGD("%02x %02x %02x %02x %02x %02x " ,
		  playbuf->data()[0],
		  playbuf->data()[1],
		  playbuf->data()[2],
		  playbuf->data()[3],
		  playbuf->data()[4],
		  playbuf->data()[5]);
	SLresult result;


#ifdef SAVE_DECODE_TO_FILE
	mSaveFile->save(  playbuf->data()  , playbuf->size());
#endif

	/* 推送另外的数据 Enqueue
		接口 SLAndroidSimpleBufferQueueItf ID
		SLresult (*Enqueue) (
			SLAndroidSimpleBufferQueueItf self,
			const void *pBuffer,        指向要播放的数据
			SLuint32 size               要播放数据的大小
		);

		SL_RESULT_BUFFER_INSUFFICIENT 内存不足
			the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
			which for this code example would indicate a programming error

		1. 不会在 SetPlayState(mIPlayerPlay, SL_PLAYSTATE_PLAYING);就回调的! 必须先enqueue一次才会启动回调
	 	2. 在不设置SL_PLAYSTATE_PLAYING的情况下(处于非playing状态) enqueue足够多的数据 也不会播放!
	 */
	//CostHelper ch ;
	result = (*mIPlayerBufferQueue)->Enqueue(mIPlayerBufferQueue, playbuf->data() , playbuf->size() );
	if (SL_RESULT_SUCCESS != result) {
		ALOGE("Enqueue ERROR !");
	}else{
		/*
		 * Note:
		 * 根据 OpenSL_ES_Specification_1.0.1.pdf 8.12 SLBufferQueueItf
		 * 在下一次回调之前不能修改buffer的数据 ！
		 *
		 * The buffers that are queued are used in place and are not required to be copied by the
		 * device, although this maybe implementation-dependent. The application developer
		 * should be aware that modifying the content of a buffer after it has been queued is
		 * undefined and can cause audio corruption.
		 *
		 * Once an enqueued buffer has finished playing, as notified by the callback notification, it
		 * is safe to delete the buffer or fill the buffer with new data and once again enqueue the
		 * buffer for playback.
		 */
		mLastQueuedBuffer = playbuf ;
		mCurrentPts = playbuf->pts();
	}
	//ALOGD("cost time %lld " , ch.Get());
	// 一次回调  只能调用一次 Enqueue

}

/* 析构顺序
	1.先调用 类 析构函数
 	2.析构类中 实例成员
 	3.基类 析构函数
 */
AudioTrack::~AudioTrack()
{
	ALOGD("~AudioTrack delete Audio Track entry %p", this );
	{
		AutoMutex l(mBufMux);
		mStop = true ;
		mBufCon->signal();
	}

	SLresult result = (*mIPlayerPlay)->SetPlayState(mIPlayerPlay, SL_PLAYSTATE_STOPPED);
	if(result != SL_RESULT_SUCCESS ) ALOGE("stop player playing error %d " , result);
	ALOGD("Stop before Destroy !");

   if (mPlayerObject != NULL) {
		ALOGD("Destroy Player Object");
		(*mPlayerObject)->Destroy(mPlayerObject);
		mPlayerObject = NULL;
		mIPlayerPlay = NULL;
		//mIPlayerMuteSolo = NULL;
		mIPlayerVolume = NULL;
	}

	if (mOutputMixObject != NULL) {
		ALOGD("Destroy Mix Object");
		(*mOutputMixObject)->Destroy(mOutputMixObject);
		mOutputMixObject = NULL;
	}

    if (mEngineObject != NULL) {
    	ALOGD("Destroy Engine Object");
        (*mEngineObject)->Destroy(mEngineObject);
        mEngineObject = NULL;
        mIEngineEngine = NULL;
    }

	{
		AutoMutex l(mBufMux);
		for( std::list<sp<Buffer>>::iterator it = mBuf.begin();
			 it != mBuf.end() ;  ){
			mBuf.erase(it++);
		}
	}

	delete mBufMux; mBufMux = NULL;
	delete mBufCon; mBufCon = NULL;
	ALOGD("~AudioTrack delete Audio Track done %p", this );

}

bool AudioTrack::setVolume( float volume ) // 0 ~ 1.0
{
	float vol = ( volume - 1.0 ) * 1000 * -1 ;
	SLresult result = (*mIPlayerVolume)->SetVolumeLevel(mIPlayerVolume, vol);// 设置音量  没有区分左右声道 毫分贝
    return SL_RESULT_SUCCESS == result;
}

bool AudioTrack::setMute( bool mute )
{
	//SLresult result = (*mIPlayerMuteSolo)->SetChannelMute(mIPlayerMuteSolo, 0, mute);
    //result = (*mIPlayerMuteSolo)->SetChannelMute(mIPlayerMuteSolo, 1, mute);
	SLresult result = SL_RESULT_SUCCESS ;
	result = (*mIPlayerVolume)->SetMute(mIPlayerVolume , mute? SL_BOOLEAN_TRUE: SL_BOOLEAN_FALSE );
    return SL_RESULT_SUCCESS == result;
}

bool AudioTrack::write(sp<Buffer> buf)
{
	if( mStarted == false ){
		SLresult result;
		result = (*mIPlayerBufferQueue)->Enqueue(mIPlayerBufferQueue, buf->data() , buf->size() );
		if (SL_RESULT_SUCCESS != result) {
			ALOGE("Enqueue ERROR !");
		}
		mStarted = true ;
		mLastQueuedBuffer = buf ;
		ALOGD("first write , enqueue ");
		return true ;
	}

	if(!mStop){
		AutoMutex l(mBufMux);
		if( mStop ) return false ;
		mBuf.push_back(buf);
		mBufCon->signal();
	}

    return true  ;
}


