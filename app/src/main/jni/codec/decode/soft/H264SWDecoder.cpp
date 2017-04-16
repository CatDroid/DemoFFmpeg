/*
 * H264SWDeocder.cpp
 *
 *  Created on: 2016年12月11日
 *      Author: hanlon
 */
#include "H264SWDecoder.h"


#include <sys/prctl.h>
#include <string>


#include "ffmpeg_common.h"
#include "project_utils.h"
#include "BufferManager.h"

extern "C"{
#include "libavutil/imgutils.h"
}


CLASS_LOG_IMPLEMENT(H264SWDecoder,"H264SWDecoder");



#define MAX_PACKET_QUEUE_SIZE 20
H264SWDecoder::H264SWDecoder():
				mpVidCtx(NULL),mDecodedFrameSize(-1),mTimeBase(-1),
				mEnqMux(NULL),mEnqSikCnd(NULL),mEnqSrcCnd(NULL),
				mEnqThID(-1),mDeqThID(-1),mStop(false) {
	TLOGT("H264SWDecoder");
}

bool H264SWDecoder::init(const AVCodecParameters* para , double timebase )
{
	int ret = 0 ;
	const AVCodec *vcodec = NULL;
	const char* codec_name = NULL;
	int codec_cap = -1 ;
	AVDictionary* opt = NULL;
	AVCodecID codec_id = AV_CODEC_ID_NONE ;


	mTimeBase = timebase ;
	mpVidCtx = avcodec_alloc_context3(NULL);
	if (!mpVidCtx){
		TLOGE("avcodec_alloc_context3 maybe out of memory");
		return false ;
	}


	ret = avcodec_parameters_to_context(mpVidCtx, para);
	if (ret < 0){
		ffmpeg_strerror(ret , "Fill the codec context ERROR");
		goto FAIL ;
	}

	vcodec = avcodec_find_decoder(mpVidCtx->codec_id); // codec_id = AV_CODEC_ID_H264 (28)
	codec_id =  vcodec->id;
	codec_name =  vcodec->name;
	codec_cap = vcodec->capabilities; // AV_CODEC_CAP_
	TLOGD("视频流编码器 %s[%d] 编码能力 0x%x" ,
		  codec_name , codec_id , codec_cap);
	//  h264 28  可以通过ffmpeg -decoders查看这个

	// 如果解码器支持 非完整的帧: 针对两帧的边界不刚好是包的边界(AVPacket)
	//if (vcodec->capabilities & AV_CODEC_CAP_TRUNCATED)
	//	mVideoCtx->flags |= AV_CODEC_FLAG_TRUNCATED; // we do not send complete frames  我们可以处理比特流
	// 设置这个可能会导致avcodec_decode_video2 got_frame不是1的时候 需要先保存数据


	av_dict_set(&opt, "threads", "auto", 0); // add an entry: 线程数 小米5(晓龙820)是4个线程
	if((ret = avcodec_open2(mpVidCtx, vcodec, &opt )) < 0){
		TLOGE("call avcodec_open2 return %d", ret);
		goto FAIL ;
	}else{
		TLOGD("编码上下文 %p, 编码器ID %d", mpVidCtx, mpVidCtx->codec_id );
	}
	mDecodedFrameSize = av_image_get_buffer_size(mpVidCtx->pix_fmt , mpVidCtx->width, mpVidCtx->height , 1 );

	mBufMgr = new BufferManager( (uint32_t)mDecodedFrameSize,  30 );


	{
		std::string meta ;
		AVDictionaryEntry *m = NULL;
		while(m=av_dict_get(opt,"",m,AV_DICT_IGNORE_SUFFIX),m!=NULL){
			meta+= m->key;
			meta+= "\t:";
			meta+= m->value;
			meta+="\n";
		}
		TLOGD("h264 decoder meta %p :\n%s", opt , meta.c_str());
	}
	av_dict_free(&opt);


	// 默认是0 解码后的frame由AVCodecContext管理
	// flags/flags2 由用户设置 AV_CODEC_FLAG_  AV_CODEC_FLAG2_ e.g AV_CODEC_FLAG_TRUNCATED
	TLOGD("AVCodecContext.refcounted_frames = %d"
				  " flags 0x%x flags2 0x%x" ,
		  mpVidCtx->refcounted_frames ,
		  mpVidCtx->flags ,mpVidCtx->flags2);


	TLOGD("H264视频帧大小 %d 帧率 %f[%d/%d] 时间基准 %f[%d/%d]  ",
		  mDecodedFrameSize,
		  av_q2d(mpVidCtx->framerate),
		  mpVidCtx->framerate.num, mpVidCtx->framerate.den,// {0, 1} when unknown.
		  mTimeBase  , mpVidCtx->time_base.num , mpVidCtx->time_base.den );
	// FrameSize 3110400 framerate 0.000000[0/1] TimeBase 0.000011[0/2]
	//
	// ??? AVCodecContext.time_base 解码已经deprecated 替代使用framerate(????) 编码必须由user设置
	//
	// mTimeBase = av_q2d(mVideoCtx->time_base) ;
	// deprecated mVideoCtx->time_base 这个是0
	// num=0 den=2 2是避免除0异常 实际是这个参数无效

	mEnqMux = new Mutex();
	mEnqSikCnd = new Condition();
	mEnqSrcCnd = new Condition();

	just4test();

	return true ;
FAIL:
	if(mpVidCtx != NULL){
		avcodec_free_context(&mpVidCtx);
		mpVidCtx = NULL;
	}
	return false ;
}

void H264SWDecoder::just4test( ) const {
// 测试代码:
	int size ;
	int size2 ;

	uint8_t *pointers[4] ;
	int linesizes[4];

// align 不能是 0 可以是4 8 16 32等对齐
	size = av_image_get_buffer_size(mpVidCtx->pix_fmt , mpVidCtx->width, mpVidCtx->height , 32 );

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	size2 = avpicture_get_size(mpVidCtx->pix_fmt , mpVidCtx->width, mpVidCtx->height);
#pragma GCC diagnostic pop

	TLOGD("pixfmt[%d], width[%d], height[%d], size[%d] size2[%d]" ,
			mpVidCtx->pix_fmt , mpVidCtx->width, mpVidCtx->height , size , size2);

#define TEST_WIDTH_NOT_ALIGN 721
#define TEST_HEIGHT 100
#define TEST_ALIGN_1 32 // 字节对齐
#define TEST_ALIGN_2 16	// 16个字节对齐
	size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P , TEST_WIDTH_NOT_ALIGN , TEST_HEIGHT , TEST_ALIGN_1 );
	TLOGD("420P [w %d h %d align %d] size %d" ,TEST_WIDTH_NOT_ALIGN , TEST_HEIGHT ,TEST_ALIGN_1 , size);
	av_image_alloc( pointers ,  linesizes , TEST_WIDTH_NOT_ALIGN , TEST_HEIGHT , AV_PIX_FMT_YUV420P,  TEST_ALIGN_1);
	TLOGD("420P [w %d h %d align %d] [%p %p %p %p] [%d %d %d %d]" ,TEST_WIDTH_NOT_ALIGN , TEST_HEIGHT ,TEST_ALIGN_1 ,
			pointers[0] , pointers[1]  ,  pointers[2]  ,  pointers[3]  ,
			linesizes[0], linesizes[1] , linesizes[2] ,  linesizes[3] );

	av_freep(&pointers[0]);

	size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P , TEST_WIDTH_NOT_ALIGN , TEST_HEIGHT , TEST_ALIGN_2 );
	TLOGD("420P [w %d h %d align %d] size %d" ,TEST_WIDTH_NOT_ALIGN , TEST_HEIGHT ,TEST_ALIGN_2 , size);
	av_image_alloc( pointers ,  linesizes , TEST_WIDTH_NOT_ALIGN , TEST_HEIGHT , AV_PIX_FMT_YUV420P,  TEST_ALIGN_2);
	TLOGD("420P [w %d h %d align %d] [%p %p %p %p] [%d %d %d %d]" ,TEST_WIDTH_NOT_ALIGN , TEST_HEIGHT ,TEST_ALIGN_2 ,
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

void H264SWDecoder::start( ) {
	int ret = 0 ;
	ret = ::pthread_create(&mEnqThID, NULL, H264SWDecoder::enqThreadEntry, (void *) this);
	if(ret < 0){
		TLOGE("Create Decode Enqueue Thread ERROR %d %d %s" , ret ,errno, strerror(errno));
		assert(ret >= 0 );
	}

	ret = ::pthread_create(&mDeqThID, NULL, H264SWDecoder::deqThreadEntry, (void *) this);
	if(ret < 0){
		TLOGE("Create Decode Dequeue Thread ERROR %d %d %s" , ret ,errno, strerror(errno));
		assert(ret >= 0 );
	}
}


void H264SWDecoder::stop() {
	mStop = true ;
}


bool H264SWDecoder::put(sp<MyPacket> packet , bool wait ){
	while(!mStop){
		AutoMutex l(mEnqMux);
		if( mPktQueue.size() == MAX_PACKET_QUEUE_SIZE ){
			TLOGW("too much video AVPacket wait!");
			// 当前已经有很多没有解码的AVPacket
			// 1.这里阻塞Muxer 等待解码完成
			// 2.可以不阻塞 立刻返回 采用丢帧方法(需要同步关键帧)
			if(wait){
				mEnqSrcCnd->wait(mEnqMux);
				continue ;
			}else{
				return false ; // 不等待
			}
		}
		mPktQueue.push_back(packet);
		mEnqSikCnd->signal();
		return true ;
	}
	return false ;// 已经stop()
}


void H264SWDecoder::enqloop(){

//	AVFrame *frame = av_frame_alloc();


//	AVPacket endpack = { // 空包 , 使用avcodec_send_packet的话直接avcodec_send_packet(ctx,NULL);
//			.data = NULL ,
//			.size = 0,
//			.dts = AV_NOPTS_VALUE,
//			.pts = AV_NOPTS_VALUE
//	};

	int ret = 0 ;
	while( ! mStop ){

		sp<MyPacket> mypkt = NULL ; // 析构时 对AVPacket unref并且返回AVPacket给Muxer的PacketManager
		AVPacket* avpkt = NULL;

		AutoMutex l(mEnqMux);
		if( ! mPktQueue.empty()  )
		{
			mypkt = mPktQueue.front();
			mPktQueue.pop_front();
			mEnqSrcCnd->signal();
		}else{
			if(mStop)break;
			mEnqSikCnd->wait(mEnqMux);
			continue;
		}


		if( mypkt.get() == NULL){
			/*
			 * avcodec_decode_video2 编程:
			 * 		把所有的解码获取出来 因为还有其他的帧还在内部
			 * 		可以检测ffmpeg解码器的capabilities
			 * 		如果具备CODEC_CAP_DELAY 那么应该(或者是必须)在解码完所有帧后
			 * 		继续循环调用解码接口
			 * 		同时(?每次)把输入包AVPacket的数据指针置为NULL,大小置为0.直到返回got_pictrue为0为止
			 *
			 * avcodec_send_packet 编程:
			 * 		调用一次发送空包 avcodec_send_packet( codec , NULL);
			 * */
			TLOGW("End Of file, send Empty Packet to Decoder");
			avcodec_send_packet(mpVidCtx,NULL);
			TLOGW("End Of file, break Loop");
			break;
			//TLOGW("get remaing frame from video decoder ");
			//avpkt = &endpack; // enter draining mode
		}else{
			avpkt = mypkt->packet();
		}



		/*
		 * avcodec_decode_video2 编程：
		 *
		 * 		当 AVCodecContext.refcounted_frames 设置为 1
		 * 		the frame 是引用计数的(reference counted) 返回的引用(returned reference)属于调用者
		 * 		所以调用者不需要的时候 使用av_frame_unref释放
		 *		调用者 在av_frame_is_writable返回1的情况下 可以安全写入数据
		 *
		 * 		当 AVCodecContext.refcounted_frames 设置为0
		 * 		返回的引用属于解码器
		 * 		只维持有效到 再次调用avcodec_decode_video2 或者 close/flush这个解码器
		 * 		调用者不能写入数据
		 *
		 * 	avcodec_receive_frame 编程:
		 * 		始终返回引用计数的视频或者音频帧
		 *
		 *  avcodec_send_packet  编程:
		 * 		可能会对AVPacke指向的AVBuffer增加引用,所以返回前不能对buffer进行操作
		 *
		 * 		如果要自己回收Buffer可以如下的方式生成AVBufferRef,而不是通过av_buffer_alloc/av_buffer_realloc
		 * 		1. data可以通过 av_malloc(size)生成
		 * 		2. AVBufferRef *av_buffer_create(uint8_t *data, int size,
		 * 					void (*free)(void *opaque, uint8_t *data),// 释放data ,回调参数是opaque
		 * 					void *opaque, int flags)
		 * 			内部创建AVBufferRef和AVBuffer
		 * 			但是AVBuffer没有开拓数据空间，而是直接用参数中的*data / size
		 */

		// ret = avcodec_decode_video2(mpVidCtx, frame, &got_frame, avpkt );

	TRY_AGAIN:
		ret = avcodec_send_packet(mpVidCtx,avpkt);
		switch(ret){
			case 0 :{
				// success avcodec_send_packet
				TLOGT(">[dts %ld pts %ld] %02x %02x %02x %02x %02x" ,
					  avpkt->dts, avpkt->pts,
					  avpkt->data?avpkt->data[0]:0xFF, // 从AVFormatContext获取 或者 放到AVCodecContext的H264不包含前引导码00000001/000001
					  avpkt->data?avpkt->data[1]:0xFF,
					  avpkt->data?avpkt->data[2]:0xFF,
					  avpkt->data?avpkt->data[3]:0xFF,
					  avpkt->data?avpkt->data[4]:0xFF
				);
				TLOGT("enqloop avcodec_send_packet done");
			}break;
			case AVERROR(EAGAIN) :{
				// TODO 两个条件变量
				// TODO 通知 avcodec_receive_frame 从EAGAIN等待条件变量中 返回
				// TODO 等待 avcodec_receive_frame 返回EAGAIN 从而唤醒自己
				TLOGW("enqloop 解码器暂时已满暂停输入\n");
				usleep(5000);
				if(!mStop) goto TRY_AGAIN ;
			}break;
			case AVERROR_EOF:{
				TLOGT("enqloop 解码器完全清空(flushed).\n");
			}break; // TODO return
			case AVERROR(EINVAL):{
				TLOGE("enqloop 参数错误\n");
				// TODO 给Player发送错误事件  Player切换到错误状态 并且反馈给应用层
			}break;
			case AVERROR(ENOMEM):{
				TLOGE("enqloop 不能把包放到内部队列\n");
				// TODO 给Player发送错误事件  Player切换到错误状态 并且反馈给应用层
			}break;
			default:{
				TLOGE("avcodec_send_packet fail ");
				ffmpeg_strerror(ret , "avcodec_send_packet:");
				// TODO
			}break;
		}
		//if( got_frame > 0 ){
			/*
			 * 是否需要保留 这次解码到的数据??  新的ffplay demo没有这样处理
			 *
			 * 是否要判断 avcodec_decode_video2 返回 已经解码这帧用到的数据 (包的边界不一定是帧边界) 然后返回到 avcodec_decode_video2
			 *
			 * ffplay的做法是
			 * 		1.每次判断decode返回 是否已经用完了这个AVPacket 没有的话 调整AVPacket的data和size继续decode
			 * 		2.已经用完这个AVPacket还没有got_frame 取下一个AVPacket
			 * 		3.如果AVPacket还没有用完 就got_frame了 调整AVPacket的data和size,先返回保存这个frame 再返回decode前不取数据了
			 *
			 * 对与视频
			 * 	av_read_frame()的例程,它可以从一个简单的包里返回一个视频帧包含的所有数据
			 * 	所以只需要判断 got_frame 的值 , 如果got_frame==0 只是代表该视频帧 还没有解码完成
			 */
		//}

	}
	//av_frame_free(&frame);
}

void H264SWDecoder::clearupPacketQueue()
{
	AutoMutex l(mEnqMux);
	TLOGI("pending mPktQueue size %lu", mPktQueue.size() );
	std::list<sp<MyPacket>>::iterator it = mPktQueue.begin();
	while(it != mPktQueue.end())
	{
		*it = NULL;
		mPktQueue.erase(it++);
	}
	TLOGI("clean up AVPacket Done!");
}

void* H264SWDecoder::enqThreadEntry(void *arg)
{
	prctl(PR_SET_NAME,"H264EnqTh");
	H264SWDecoder* decoder = (H264SWDecoder*)arg;
	decoder->enqloop();
	return NULL;
}

void H264SWDecoder::deqloop(){

	AVFrame* pFrame = av_frame_alloc();
	int ret = 0 ;

	while ( !mStop) {
		ret = avcodec_receive_frame(mpVidCtx, pFrame); // non-block
		switch (ret) {
			case 0:{ //成功
				if (pFrame->pts == AV_NOPTS_VALUE) {
					pFrame->pts = av_frame_get_best_effort_timestamp(pFrame);
				}

				AVColorSpace cs = av_frame_get_colorspace(pFrame) ; // 目前看都是 AVCOL_SPC_UNSPECIFIED 2
				AVPixelFormat pixfmt = mpVidCtx->pix_fmt ; // 这个参数重要

				TLOGD("<[pts %" PRId64 " pkt_dts %" PRId64 " pkt_pts %" PRId64 "]"
						" 颜色空间 %d  像素格式 %d "
						" 是否引用计数 %d 可写 %d "
						" 四个平面 %p[%d] %p[%d] %p[%d] %p[%d]" ,
					  pFrame->pts, pFrame->pkt_dts ,  pFrame->pkt_pts,
					  cs, pixfmt,
					  mpVidCtx->refcounted_frames,  av_frame_is_writable(pFrame),
					  (void*)pFrame->data[0], pFrame->linesize[0],
					  (void*)pFrame->data[1], pFrame->linesize[1],
					  (void*)pFrame->data[2], pFrame->linesize[2],
					  (void*)pFrame->data[3], pFrame->linesize[3]
				);
				// mVideoCtx->pix_fmt 的可能是
				//  Codec PixelFormat 0 AV_PIX_FMT_YUV420P
				//	Codec PixelFormat 5 AV_PIX_FMT_YUV444P
				// 有些mp4文件是yuv444(ubuntu屏幕录制) yuv422（ubuntu摄像头)

				//TLOGD("video pts decode %ld " , frame->pts );
				sp<Buffer> buf = mBufMgr->pop();
				/**
				 * 注意:linesize @ AVFrame
				 * 		For video, 各个平面中的行的大小
				 * 		For audio, 每个平面的大小
				 * 					对于planar类型(e.g S16P FLTP) 左右声道,分别放在两个平面内AVFrame.data[0] AVFrame.data[1]
				 * 					对于packet类型(e.g S16 FLT) 左右声道,放在一起 只有AVFrame.data[0]
				 *
				 * 		For packet audio, 只有linesize[0].
				 * 		For planar audio, 每个 通道/平面 的大小必须一样
				 *
				 * 		linesize 可能大于有用数据的大小,为了优化性能加入格外的对齐
				 * 		对于视频 linesize 应该是 符合CPU执行性能的对齐数 的 倍数 比如 现代桌面CPU是 16 or 32
				 * 		对于某些codec需要这样的对齐,但有些codec则无影响
				 *
				 * 	TODO 如果渲染sharder支持YUV三个平面作为sharder的话 可以不用在这里合并成一片内存
				 */
				av_image_copy_to_buffer(buf->data(),
										mDecodedFrameSize ,
										(const uint8_t *const *) pFrame->data,
										pFrame->linesize ,
										mpVidCtx->pix_fmt ,
										mpVidCtx->width ,
										mpVidCtx->height ,
										1);// AVPicture ---> AVFrame

//				保存到文件
//				static WriteFile* testfile = NULL;
//				if(testfile == NULL ){
//					testfile = new WriteFile("/mnt/sdcard/myh264test2.yuv");
//					testfile->write(avbuf , mpVidCtx->width*mpVidCtx->height*3/2 );
//					delete testfile;
//					TLOGD("write testfile ok");
//				}

				buf->pts() = (int64_t) (pFrame->pts * mTimeBase * 1000);  // ms
				buf->height() = mpVidCtx->height ;
				buf->width() = mpVidCtx->width ;
				buf->size() = mDecodedFrameSize ;
				buf->fmt() = mpVidCtx->pix_fmt;
				av_frame_unref(pFrame);
				// TODO 给到渲染线程
				//mRender->renderVideo()
			}break;
			case AVERROR_EOF:{
				// TODO 告诉Player文件已经解码完毕 可能等待渲染结束
				TLOGT("deqloop 解码器完全清空, 没有更多帧输出.\n");
				// TODO 告诉渲染线程文件已经结束
				// mRender->renderVideo(NULL) ;
			}break; // TODO return
			case AVERROR(EAGAIN):{
				// TODO 目前只是休眠
				// 根据:  https://ffmpeg.org/doxygen/3.1/group__lavc__encdec.html
				// 引述:
				// the only guarantee is that an AVERROR(EAGAIN) return value on a send/receive call on one end
				// implies that a receive/send call on the other end will succeed
				// 即是: 只保证一端返回AVERROR(EAGAIN)意味另外一端可以返回成功
				TLOGW("deqloop 解码器暂时无输出\n");
				usleep(4000);
			}break;
			case AVERROR(EINVAL):{
				TLOGE("deqloop 参数错误\n");
				// TODO 给Player发送错误事件  Player切换到错误状态 并且反馈给应用层
			}break;
			default:{

			}break;
		}
	}

	av_frame_free(&pFrame);
}

void* H264SWDecoder::deqThreadEntry(void *arg) {
	prctl(PR_SET_NAME,"H264DeqTh");
	H264SWDecoder* decoder = (H264SWDecoder*)arg;
	decoder->deqloop();
	return NULL;
}

H264SWDecoder::~H264SWDecoder()
{
	if(mEnqThID != -1) {
		{
			AutoMutex l(mEnqMux);
			mEnqSrcCnd->signal();
			mEnqSikCnd->signal();
		}
		pthread_join(mEnqThID, NULL);
		clearupPacketQueue();
		TLOGD("H264SWDecoder stop done");
	}else{
		TLOGD("Decode Thread not start yet ");
	}

	if(mDeqThID != -1) {
		// TODO  dequeue线程可能挂在render队列上
		pthread_join(mDeqThID, NULL);
		clearupPacketQueue();
		TLOGD("H264SWDecoder stop done");
	}else{
		TLOGD("Decode Thread not start yet ");
	}


	if(mEnqMux!=NULL) 	 { delete mEnqMux;		mEnqMux = NULL ;}
	if(mEnqSrcCnd!=NULL) { delete mEnqSrcCnd;	mEnqSrcCnd = NULL; }
	if(mEnqSikCnd!=NULL) { delete mEnqSikCnd;	mEnqSikCnd = NULL; }

	if(mpVidCtx != NULL){
		avcodec_free_context(&mpVidCtx);
		mpVidCtx = NULL;
	}

	TLOGT("~H264SWDecoder");
}