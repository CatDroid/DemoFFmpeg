/*
 * AACSWDecoder.cpp
 *
 *  Created on: 2016年12月11日
 *      Author: hanlon
 */


#include "AACSWDecoder.h"
#define LOG_TAG "AACSWDecoder"
#include "jni_common.h"
#include "ffmpeg_common.h"
#include "project_utils.h"
#include <errno.h>

//#define SAVE_DECODE_TO_FILE
#define MAX_PACKET_QUEUE_SIZE 43

AACSWDecoder::AACSWDecoder(AVCodecParameters* para, double timebase):mStop(false),mTimeBase(timebase),mSwrCtx(NULL)
{
	int ret = 0 ;
	mAudioCtx = avcodec_alloc_context3(NULL);
	if (!mAudioCtx){
		ALOGE("avcodec_alloc_context3 maybe out of memory");
		return ;
	}
	ret = avcodec_parameters_to_context(mAudioCtx, para);
	if (ret < 0){
		ffmpeg_strerror(ret , "Fill the codec context ERROR");
		return ;
	}

	const AVCodec *acodec = avcodec_find_decoder(mAudioCtx->codec_id);
	AVCodecID codec_id =  acodec->id; // AV_CODEC_ID_H264  AV_CODEC_ID_AAC
	const char* codec_name =  acodec->name;
	ALOGD("audio stream codec %s %d " , codec_name , codec_id );
	if((ret = avcodec_open2(mAudioCtx, acodec, NULL)) < 0){
		ALOGE("call avcodec_open2 return %d", ret);
		return ;
	}else{


		/*
		*	如果不是0 那么由avcodec_decode_video2() and avcodec_decode_audio4()
		*	解码输出的audio/video frame	是引用计数的(reference-counted) 和无效期有效的(valid indefinitely)
		*   调用者必须在不需要的时候调用av_frame_unref()来释放
		* 	如果是0  那么解码输出frame不能由调用者释放 以及解码输出frame只会有效到下一次调用 avcodec_decode_xxxx()
		*
		*	如果使用 avcodec_receive_frame() 这个总是自动使能
		* - encoding: 没有使用
		* - decoding: 调用者在调用avcodec_open2()之前设置
		*/
		ALOGD("AVCodecContext.refcounted_frames = %d " , mAudioCtx->refcounted_frames );// 目前来看是0
		ALOGD("bits_per_coded_sample = %d bits_per_raw_sample = %d " ,
			  mAudioCtx->bits_per_coded_sample, mAudioCtx->bits_per_raw_sample );
	}

	mQueueMutex = new Mutex();
	mSinkCond = new Condition();
	mSourceCond = new Condition() ;

	//输出音频数据大小，一定小于输出内存。
	int out_linesize; // 每个平面???
	// TODO 需要根据 采样频率和通道数来创建 AudioTrack!
	mDecodedFrameSize =av_samples_get_buffer_size(&out_linesize, mAudioCtx->channels,mAudioCtx->frame_size,AV_SAMPLE_FMT_S16 , 1);
	ALOGD("calculated linesize %d, buffer size %d " , out_linesize,mDecodedFrameSize);
	assert(out_buffer_size>0);
	mBM = new BufferManager(mDecodedFrameSize ,  30 );

#ifdef SAVE_DECODE_TO_FILE
	mSaveFile = new SaveFile("/mnt/sdcard/decode.pcm");
#endif
}


void AACSWDecoder::loop( ){


	AVFrame *frame = av_frame_alloc();
	int got_frame = 0 ;


	int ret = 0 ;
	while(! mStop ){


		sp<MyPacket> mypkt = NULL ;
		{
			AutoMutex l(mQueueMutex);
			if( mPacketQueue.empty() == false )
			{
				mypkt = mPacketQueue.front();
				mPacketQueue.pop_front();
				mSourceCond->signal();
			}else{
				if(mStop)break;
				mSinkCond->wait(mQueueMutex);
				continue;
			}

		}

		{
			ALOGD(">[dts %ld pts %ld] %02x %02x %02x %02x %02x" ,
				  mypkt->packet()->dts,
				  mypkt->packet()->pts,
				  mypkt->packet()->data[0],
				  mypkt->packet()->data[1],
				  mypkt->packet()->data[2],
				  mypkt->packet()->data[3],
				  mypkt->packet()->data[4]
				);
			// @deprecated Use avcodec_send_packet() and avcodec_receive_frame().
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
			ret  = avcodec_decode_audio4(mAudioCtx, frame, &got_frame, mypkt->packet());
//#pragma GCC diagnostic pop
			if( ret < 0 ){
				ALOGE("avcodec_decode_video2 fail ");
				ffmpeg_strerror(ret , "avcodec_decode_video2:");
				break;
			}
			if( got_frame > 0 ) {
				// frame_number 到目前为止 解码器已返回的总帧数
				if (frame->pts == AV_NOPTS_VALUE) {
					frame->pts = av_frame_get_best_effort_timestamp(frame);
				}

				ALOGD("<[pts %lld pkt_dts %lld pkt_pts %lld] "
							  "total number of frames: %d linesize[0]: %d " ,
					  frame->pts,frame->pkt_dts , frame->pkt_pts,
					  mAudioCtx->frame_number , frame->linesize[0] ); // 8192 = 1024(每个通道一个帧含有多少个sample)*2 * 4(Float)


				sp<Buffer> buf = mBM->pop();
				if(mSwrCtx == NULL ){
					/*
					 * 在使用ffmpeg解码aac的时候，如果使用avcodec_decode_audio4函数解码
					 * 那么解码出来的会是AV_SAMPLE_FMT_FLTP 格式的数据( float, 4bit , planar)
					 * 如希望得到16bit的数据（如AV_SAMPLE_FMT_S16P数据），那么需要转换
					 */
					mSwrCtx = swr_alloc();
					mSwrCtx = swr_alloc_set_opts(mSwrCtx,
												 mAudioCtx->channel_layout,// 应该跟AudioTrack一样
											     AV_SAMPLE_FMT_S16,        // 应该跟AudioTrack一样
												 mAudioCtx->sample_rate ,  // 应该跟AudioTrack一样

												 mAudioCtx->channel_layout,// 跟解码器一样
												 mAudioCtx->sample_fmt,
												 mAudioCtx->sample_rate ,
											     0,
												 NULL);
					swr_init(mSwrCtx);
				}
				uint8_t* lineaddr[] = { buf->data() };
				// swr_convert 返回每个通道的样本数    *2通道*2字节
				int32_t len = swr_convert(mSwrCtx, lineaddr , mDecodedFrameSize ,(const uint8_t **)frame->data , frame->nb_samples);
				if( len > 0 ){
					int resampled_data_size = len * mAudioCtx->channels  * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);//每声道采样数 x 声道数 x 每个采样字节数
					ALOGD( "len = %d resampled_data_size = %d  mDecodedFrameSize = %d " ,len, resampled_data_size,  mDecodedFrameSize );
					buf->size() = mDecodedFrameSize ;
					buf->pts() = frame->pts * mTimeBase ;

#ifdef SAVE_DECODE_TO_FILE
					mSaveFile->save(  buf->data()  , buf->size());
#endif
					mpRender->renderAudio( buf ) ;
				}else{
					ffmpeg_strerror(len,"swr_convert");
				}

				av_frame_unref(frame);
			}else{
				ALOGE("AAC SWDecoder get Nothing !");
			}
		}
		mypkt = NULL;
	}
	av_frame_free(&frame);


}

void* AACSWDecoder::decodeThread(void* arg)
{
	AACSWDecoder* decoder = (AACSWDecoder*)arg;
	decoder->loop();

	return NULL;
}

void AACSWDecoder::start(RenderThread* rt )
{


	mpRender = rt ;
	int ret = 0 ;
	ret = ::pthread_create(&mDecodeTh, NULL, AACSWDecoder::decodeThread, (void*)this  );
	if(ret < 0){
		ALOGE("create Decode Thread ERROR %d %d %s" , ret ,errno, strerror(errno));
	}
}

void AACSWDecoder::stop()
{
	mStop = true ;
	if(mDecodeTh != -1){
        {
            Mutex::Autolock l(mQueueMutex);
            mSourceCond->signal();
            mSinkCond->signal();
        }
		pthread_join(mDecodeTh, NULL);
		clearupPacketQueue();
        delete mQueueMutex ;    mQueueMutex = NULL ;
        delete mSinkCond;       mSinkCond = NULL;
        delete mSourceCond;     mSourceCond = NULL;
		ALOGD("AACSWDecoder stop done ");

	}else{
		ALOGD("AACSWDecoder not start yet ");
	}
}




bool AACSWDecoder::put(sp<MyPacket> packet ){

	while(!mStop){
		AutoMutex l(mQueueMutex);
		if(mStop) return false ;
		if( mPacketQueue.size() == MAX_PACKET_QUEUE_SIZE ){
			ALOGW("too much audio AVPacket wait!");
			mSourceCond->wait(mQueueMutex);
			continue ;
			// 当前已经有很多没有解码的AVPacket 这里阻塞Muxer 等待解码完成
			// 可以不阻塞 立刻返回  采用丢帧方法
		}
		mPacketQueue.push_back(packet);
		mSinkCond->signal();
		return true ;
	}
	return false ;
}


void AACSWDecoder::clearupPacketQueue()
{
	AutoMutex l(mQueueMutex);
	std::list<sp<MyPacket>>::iterator it = mPacketQueue.begin();
	ALOGI("pending mPacketQueue size %d ", mPacketQueue.size() );
	while(it != mPacketQueue.end()) {
		mPacketQueue.erase(it++);
	}
	ALOGI("clean up AVPacket Done!");
}


AACSWDecoder::~AACSWDecoder(){
	ALOGD("~AACSWDecoder %p" , this);
}

