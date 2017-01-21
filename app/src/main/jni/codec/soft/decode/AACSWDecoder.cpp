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


#define MAX_PACKET_QUEUE_SIZE 43

AACSWDecoder::AACSWDecoder(AVCodecParameters* para):mStop(false)
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
}


void AACSWDecoder::loop( ){

	AVPacket* packet ;
	AVFrame *frame = av_frame_alloc();
	int got_frame = 0 ;


	int ret = 0 ;
	while(! mStop ){
		packet = NULL ;

        {
            Mutex::Autolock l(mQueueMutex);
            if( mPacketQueue.empty() == false )
            {
                packet = mPacketQueue.front();
                mPacketQueue.pop_front();

                /*如果生产者buffer已经满了*/mSourceCond->signal();
            }else{
                mSinkCond->wait(mQueueMutex);// 消费者buffer空
                continue;
            }
        }

		{
			// @deprecated Use avcodec_send_packet() and avcodec_receive_frame().
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
			ret  = avcodec_decode_audio4(mAudioCtx, frame, &got_frame, packet);
//#pragma GCC diagnostic pop
			if( ret < 0 ){
				ALOGE("avcodec_decode_video2 fail ");
				ffmpeg_strerror(ret , "avcodec_decode_video2:");
				break;
			}
			if( got_frame > 0 ){
				// frame_number 到目前为止 解码器已返回的总帧数
				ALOGD("total number of frames: %d cur: %d " ,
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
												 mAudioCtx->channel_layout,
											     AV_SAMPLE_FMT_S16,
												 mAudioCtx->sample_rate ,
												 mAudioCtx->channel_layout,
												 mAudioCtx->sample_fmt,
												 mAudioCtx->sample_rate ,
											     0,
												 NULL);
					swr_init(mSwrCtx);
				}
				uint8_t* lineaddr[] = {buf->data()};
				int32_t len = swr_convert(mSwrCtx, lineaddr , mDecodedFrameSize ,(const uint8_t **)frame->data , frame->nb_samples);

				int resampled_data_size = len * mAudioCtx->channels  * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);//每声道采样数 x 声道数 x 每个采样字节数

				ALOGD( "resampled_data_size = %d  mDecodedFrameSize = %d " ,resampled_data_size,  mDecodedFrameSize );
				buf->size() = mDecodedFrameSize ;
				buf->pts() = packet->pts ;

				mpRender->renderAudio( buf ) ;
				av_frame_unref(frame);
			}

			av_packet_unref(packet);
		}
	}

	clearupPacketQueue();

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
        delete mQueueMutex ;    mQueueMutex = NULL ;
        delete mSinkCond;       mSinkCond = NULL;
        delete mSourceCond;     mSourceCond = NULL;
		ALOGD("AACSWDecoder stop done ");
	}else{
		ALOGD("AACSWDecoder not start yet ");
	}
}

bool AACSWDecoder::isPacketQueueFull()
{
	if( mPacketQueue.size() >= MAX_PACKET_QUEUE_SIZE ){
		return true ;
	}else{
		return false ;
	}
}

bool AACSWDecoder::put(AVPacket* packet )
{
	if( mStop ) return false ;

    Mutex::Autolock l(mQueueMutex);
	int size = mPacketQueue.size() ;
	if( size == MAX_PACKET_QUEUE_SIZE ){
        mSourceCond->wait(mQueueMutex);
		if( mStop ){ // exit
			return false ;
		}
	}

	AVPacket* one = av_packet_alloc();
	*one = *packet;
	mPacketQueue.push_back(one);
    mSinkCond->signal();
	return true ;
}

bool AACSWDecoder::put(sp<Buffer> packet ){

	while(!mStop){
		AutoMutex l(mQueueMutex);
		if( mPacketQueue.size() == MAX_PACKET_QUEUE_SIZE ){
			mSourceCond->wait(mQueueMutex);
			continue ;
		}
		mPacketQueue.push_back(packet);
		mSinkCond->signal();
		return true ;
	}
	return false ;
}


void AACSWDecoder::clearupPacketQueue()
{
    Mutex::Autolock l(mQueueMutex);

	std::list<AVPacket*>::iterator it = mPacketQueue.begin();
	while(it != mPacketQueue.end())
	{
		av_packet_unref(*it);
		mPacketQueue.erase(it++);
		ALOGI("clean up !");
	}

}


AACSWDecoder::~AACSWDecoder(){
	ALOGD("~AACSWDecoder %p" , this);
}

