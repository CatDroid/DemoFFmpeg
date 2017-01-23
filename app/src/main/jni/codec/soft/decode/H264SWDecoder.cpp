/*
 * H264SWDeocder.cpp
 *
 *  Created on: 2016年12月11日
 *      Author: hanlon
 */

#include "H264SWDecoder.h"
#define LOG_TAG "H264SWDecoder"
#include "jni_common.h"
#include "ffmpeg_common.h"
#include "project_utils.h"



extern "C"{
#include "libavutil/imgutils.h"
}

#define MAX_PACKET_QUEUE_SIZE 20
H264SWDecoder::H264SWDecoder(AVCodecParameters* para , double timebase)
				:mVideoCtx(NULL),mpRender(NULL),mDecodeTh(-1),mStop(false),mTimeBase(timebase)
{
	int ret = 0 ;
	const AVCodec *vcodec = NULL;
	const char* codec_name = NULL;

	AVCodecID codec_id = AV_CODEC_ID_NONE ;

	mVideoCtx = avcodec_alloc_context3(NULL);
	if (!mVideoCtx){
		ALOGE("avcodec_alloc_context3 maybe out of memory");
		return ;
	}
	ret = avcodec_parameters_to_context(mVideoCtx, para);
	if (ret < 0){
		ffmpeg_strerror(ret , "Fill the codec context ERROR");
		goto FAIL ;
	}

	vcodec = avcodec_find_decoder(mVideoCtx->codec_id);
	codec_id =  vcodec->id;
	codec_name =  vcodec->name;
	ALOGD("video stream codec %s %d " , codec_name , codec_id );
	if((ret = avcodec_open2(mVideoCtx, vcodec, NULL)) < 0){
		ALOGE("call avcodec_open2 return %d", ret);
		goto FAIL ;
	}else{
		ALOGD("avcodec open videoctx=%p, codecid=%d", mVideoCtx, mVideoCtx->codec_id );
	}
	mDecodedFrameSize = av_image_get_buffer_size(mVideoCtx->pix_fmt , mVideoCtx->width, mVideoCtx->height , 1 );

	mBM = new BufferManager( (uint32_t)mDecodedFrameSize,  30 );
	/*
	 * time_base.num是分子
	 * time_base.den是分母
	 * av_q2d 就是 把 num/den
	 */
	//mTimeBase = av_q2d(mVideoCtx->time_base) ; // deprecated mVideoCtx->time_base 这个是0


	ALOGD("FrameSize %d TimeBase %f[%d/%d]  ", mDecodedFrameSize,  mTimeBase , mVideoCtx->time_base.num , mVideoCtx->time_base.den );
	mQueueMutex = new Mutex();
	mSinkCond = new Condition();
	mSourceCond = new Condition();

	just4test();

	return ;
FAIL:
	if(mVideoCtx != NULL){
		avcodec_free_context(&mVideoCtx);
		mVideoCtx = NULL;
	}
	return ;
}


void H264SWDecoder::just4test( ) const {
// 测试代码:
	int size ;
	int size2 ;

	uint8_t *pointers[4] ;
	int linesizes[4];

// align 不能是 0 可以是4 8 16 32等对齐
	size = av_image_get_buffer_size(mVideoCtx->pix_fmt , mVideoCtx->width, mVideoCtx->height , 32 );

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	size2 = avpicture_get_size(mVideoCtx->pix_fmt , mVideoCtx->width, mVideoCtx->height);
#pragma GCC diagnostic pop

	ALOGD("pixfmt[%d], width[%d], height[%d], size[%d] size2[%d]" ,
			mVideoCtx->pix_fmt , mVideoCtx->width, mVideoCtx->height , size , size2);

#define TEST_WIDTH_NOT_ALIGN 721
#define TEST_HEIGHT 100
#define TEST_ALIGN_1 32 // 字节对齐
#define TEST_ALIGN_2 16	// 16个字节对齐
	size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P , TEST_WIDTH_NOT_ALIGN , TEST_HEIGHT , TEST_ALIGN_1 );
	ALOGD("420P [w %d h %d align %d] size %d" ,TEST_WIDTH_NOT_ALIGN , TEST_HEIGHT ,TEST_ALIGN_1 , size);
	av_image_alloc( pointers ,  linesizes , TEST_WIDTH_NOT_ALIGN , TEST_HEIGHT , AV_PIX_FMT_YUV420P,  TEST_ALIGN_1);
	ALOGD("420P [w %d h %d align %d] [%p %p %p %p] [%d %d %d %d]" ,TEST_WIDTH_NOT_ALIGN , TEST_HEIGHT ,TEST_ALIGN_1 ,
			pointers[0] , pointers[1]  ,  pointers[2]  ,  pointers[3]  ,
			linesizes[0], linesizes[1] , linesizes[2] ,  linesizes[3] );

	av_freep(&pointers[0]);

	size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P , TEST_WIDTH_NOT_ALIGN , TEST_HEIGHT , TEST_ALIGN_2 );
	ALOGD("420P [w %d h %d align %d] size %d" ,TEST_WIDTH_NOT_ALIGN , TEST_HEIGHT ,TEST_ALIGN_2 , size);
	av_image_alloc( pointers ,  linesizes , TEST_WIDTH_NOT_ALIGN , TEST_HEIGHT , AV_PIX_FMT_YUV420P,  TEST_ALIGN_2);
	ALOGD("420P [w %d h %d align %d] [%p %p %p %p] [%d %d %d %d]" ,TEST_WIDTH_NOT_ALIGN , TEST_HEIGHT ,TEST_ALIGN_2 ,
			pointers[0] , pointers[1]  ,  pointers[2]  ,  pointers[3]  ,
			linesizes[0], linesizes[1] , linesizes[2] ,  linesizes[3] );
	av_freep(&pointers[0]);

/*
	pixfmt[0], width[1280], height[960], size[1843200] size2[1843200]
	420P [w 721 h 100 align 32] size 112000
	420P [w 721 h 100 align 32] [0x7f65bdc000 0x7f65bedf80 0x7f65bf2a80 0x0] [736 384 384 0]
	420P [w 721 h 100 align 16] size 110400
	420P [w 721 h 100 align 16] [0x7f65bdc000 0x7f65bedf80 0x7f65bf2760 0x0] [736 368 368 0]
		 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 Y:ceil(721/16)*16 = 736
		 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 U/V:ceil(721/2)=361 ceil(361/16)*16 = 368
 */}

void H264SWDecoder::start(RenderThread* rt )
{
	mpRender = rt ;
	int ret = 0 ;
	ret = ::pthread_create(&mDecodeTh, NULL, H264SWDecoder::decodeThread, (void*)this  );
	if(ret < 0){
		ALOGE("create Decode Thread ERROR %d %d %s" , ret ,errno, strerror(errno));
	}
}


void H264SWDecoder::stop()
{
	mStop = true ;
	if(mDecodeTh != -1) {
		{
			AutoMutex l(mQueueMutex);
			mSourceCond->signal();
			mSinkCond->signal();
		}
		pthread_join(mDecodeTh, NULL);
		clearupPacketQueue();
		ALOGD("H264SWDecoder stop done");
	}else{
		ALOGD("Decode Thread not start yet ");
	}

}


bool H264SWDecoder::put(sp<MyPacket> packet ){

	while(!mStop){
		AutoMutex l(mQueueMutex);
		if( mPacketQueue.size() == MAX_PACKET_QUEUE_SIZE ){
			ALOGW("too much video AVPacket wait!");
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


void H264SWDecoder::loop( ){


	AVFrame *frame = av_frame_alloc();
	int got_frame = 0 ;


	int ret = 0 ;
	while( ! mStop ){
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


		CostHelper* n = new CostHelper();

		AVPacket* packet = mypkt->packet();
		ALOGD(">[dts %ld pts %ld] %02x %02x %02x %02x %02x" ,
			  mypkt->packet()->dts,
			  mypkt->packet()->pts,
			  mypkt->packet()->data[0],
			  mypkt->packet()->data[1],
			  mypkt->packet()->data[2],
			  mypkt->packet()->data[3],
			  mypkt->packet()->data[4]
		);

		// avcodec_send_packet() and avcodec_receive_frame() 异步方式
		ret = avcodec_decode_video2(mVideoCtx, frame, &got_frame, packet );

		if( ret < 0 ){
			ALOGE("avcodec_decode_video2 fail ");
			ffmpeg_strerror(ret , "avcodec_decode_video2:");
			break;
		}
		if( got_frame > 0 ){
			if (frame->pts == AV_NOPTS_VALUE) {
				frame->pts = av_frame_get_best_effort_timestamp(frame);
			}

			// YUV colorspace type.
			AVColorSpace cs = av_frame_get_colorspace(frame) ; // AVCOL_SPC_UNSPECIFIED
			AVPixelFormat pixfmt = mVideoCtx->pix_fmt ; // AV_PIX_FMT_YUV420P
			ALOGD("<[pts %lld pkt_dts %lld pkt_pts %lld] Frame color space %d  Codec PixelFormat %d [%p %p %p %p] [%d %d %d %d]" ,
				  frame->pts,frame->pkt_dts , frame->pkt_pts,
				  cs  , pixfmt ,
				  frame->data[0], frame->data[1] , frame->data[2] , frame->data[3],
				  frame->linesize[0],frame->linesize[1],frame->linesize[2],frame->linesize[3]
				);

			ALOGD("decode diff 1  %lld us " , n->Get() );

			//ALOGD("video pts decode %ld " , frame->pts );
			sp<Buffer> buf = mBM->pop();
			/**
			 * For video, 每个图片data的函数
			 * For audio, 每个平面data的大小
			 *
			 * For audio, 只有linesize[0].
			 * For planar audio, 每个通道平面的大小必须一样
			 *
			 * 对于视频 linesizes 应该是 符合CPU执行性能的对齐数 的 倍数 比如 现代桌面CPU是 16 or 32
			 * 对于某些codec需要这样的对齐,但有些codec则无影响
			 *
			 * 注意: linesize 可能大于有用数据的大小 , 为了优化性能加入格外的对齐
			 */
			av_image_copy_to_buffer(buf->data(),
								mDecodedFrameSize ,
								frame->data ,
								frame->linesize ,
								mVideoCtx->pix_fmt ,
								mVideoCtx->width ,
								mVideoCtx->height ,
								1);// AVPicture ---> AVFrame


//			static WriteFile* testfile = NULL;
//			if(testfile == NULL ){
//				testfile = new WriteFile("/mnt/sdcard/myh264test2.yuv");
//				testfile->write(avbuf , mVideoCtx->width*mVideoCtx->height*3/2 );
//				delete testfile;
//				ALOGD("write testfile ok");
//			}
			ALOGD("decode diff 2  %lld us " , n->Get() );
			buf->pts() = frame->pts * mTimeBase * 1000  ;  // ms
			buf->height() = mVideoCtx->height ;
			buf->width() = mVideoCtx->width ;
			buf->size() = mDecodedFrameSize ;
			mpRender->renderVideo(buf) ;
			av_frame_unref(frame);
			ALOGD("decode diff 3  %lld us " , n->Get() );
		}else{
			ALOGD("Got Nothing");
		}
		packet = NULL;
	}

	av_frame_free(&frame);

}

void H264SWDecoder::clearupPacketQueue()
{
	AutoMutex l(mQueueMutex);
	std::list<sp<MyPacket>>::iterator it = mPacketQueue.begin();
	while(it != mPacketQueue.end())
	{
		*it = NULL;
		mPacketQueue.erase(it++);
		ALOGI("clean up AVPacket !");
	}
}

void* H264SWDecoder::decodeThread(void* arg)
{
	H264SWDecoder* decoder = (H264SWDecoder*)arg;
	decoder->loop();

	return NULL;
}

H264SWDecoder::~H264SWDecoder()
{
	delete mQueueMutex; mQueueMutex = NULL ;
	delete mSourceCond; mSourceCond = NULL;
	delete mSinkCond; 	mSinkCond = NULL;

	if(mVideoCtx != NULL){
		avcodec_free_context(&mVideoCtx);
		mVideoCtx = NULL;
	}
}
