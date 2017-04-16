/*
 * AACSWDecoder.cpp
 *
 *  Created on: 2016年12月11日
 *      Author: hanlon
 */

#include "AACSWDecoder.h"

#include <sys/prctl.h>

#include "ffmpeg_common.h"


//#define SAVE_DECODE_TO_FILE
#define MAX_PACKET_QUEUE_SIZE 43

CLASS_LOG_IMPLEMENT(AACSWDecoder,"AACSWDecoder");

AACSWDecoder::AACSWDecoder():
		mpAudCtx(NULL),mpSwrCtx(NULL),
		mDecodedFrameSize(-1),mTimeBase(-1),
		mEnqMux(NULL),mEnqSikCnd(NULL),mEnqSrcCnd(NULL),
		mStop(false),mEnqThID(-1),mDeqThID(-1) {
	TLOGT("AACSWDecoder");
}

bool AACSWDecoder::init(const AVCodecParameters *para, double timebase) {
	int ret = 0 ;

	mTimeBase = timebase ;
	mpAudCtx = avcodec_alloc_context3(NULL);
	if (!mpAudCtx){
		TLOGE("avcodec_alloc_context3 maybe out of memory");
		return false;
	}
	ret = avcodec_parameters_to_context(mpAudCtx, para);
	if (ret < 0){
		ffmpeg_strerror(ret , "Fill the codec context ERROR");
		TLOGE("Fill the codec context ERROR");
		return false;
	}

	const AVCodec *acodec = avcodec_find_decoder(mpAudCtx->codec_id);
	AVCodecID codec_id =  acodec->id; // AV_CODEC_ID_H264  AV_CODEC_ID_AAC
	const char* codec_name =  acodec->name;
	int codec_cap = acodec->capabilities;
	TLOGD("音频流编码器 %s[%d] 编码能力 0x%x" , codec_name , codec_id , codec_cap);
	//  dump编码器能力 h264 0x3022 AAC  0x402
	if( codec_cap & AV_CODEC_CAP_SLICE_THREADS ) TLOGT("supports frame-level multithreading.");
	if( codec_cap & AV_CODEC_CAP_FRAME_THREADS) TLOGT("supports slice-based (or partition-based) multithreading.");
	if( codec_cap & AV_CODEC_CAP_DELAY)TLOGT("The decoder has a non-zero delay and needs to be fed with avpkt->data=NULL,\n"
					  "* avpkt->size=0 at the end to get the delayed data until the decoder no longer\n"
					  "* returns frames.");
	if( codec_cap & AV_CODEC_CAP_DR1)TLOGT("AV_CODEC_CAP_DR1 ");

	if( codec_cap & AV_CODEC_CAP_CHANNEL_CONF )TLOGT("Codec should fill in channel configuration and samplerate instead of container");

	// 临时修改线程名字  因为ffmpeg内部会启动线程
	char oldName[256]; memset(oldName,0,sizeof(oldName));
	prctl(PR_GET_NAME, oldName); // 名字的长度最大为15字节，且应该以'\0'结尾

	char newName[16]; memset(newName, 0, 16);
	snprintf(newName, 16, "AAC_%p", this); // H264_0x7f9abf30
	prctl(PR_SET_NAME, newName);

	if((ret = avcodec_open2(mpAudCtx, acodec, NULL)) < 0){
		TLOGE("call avcodec_open2 return %d", ret);
		return false;
	}else{
		/*
		 * 注意: AVCodecContext.refcounted_frames
		 * 		如果不是0 那么由avcodec_decode_video2() and avcodec_decode_audio4()
		 * 		解码输出的audio/video frame 是引用计数的(reference-counted) 和无限期有效的(valid indefinitely)
		 * 		调用者必须在不需要的时候调用av_frame_unref()来释放
		 * 		如果是0  那么解码输出frame不能由调用者释放 以及解码输出frame只会有效到下一次调用 avcodec_decode_xxxx()
		 *
		 * 		如果使用 avcodec_receive_frame() 这个总是自动使能
		 * 		- encoding: 没有使用
		 * 		- decoding: 调用者在调用avcodec_open2()之前设置
		*/
		TLOGD("AVCodecContext.refcounted_frames = %d " , mpAudCtx->refcounted_frames );// 目前来看是0
		TLOGD("bits_per_coded_sample = %d bits_per_raw_sample = %d " ,
			  mpAudCtx->bits_per_coded_sample,
			  mpAudCtx->bits_per_raw_sample );
	}

	mEnqMux = new Mutex();
	mEnqSikCnd = new Condition();
	mEnqSrcCnd = new Condition() ;


	int out_linesize = 0 ;
	// 每个 通道/平面 的大小
	// 如果format是S16P FLTP等planar类型 每个平面必须长度一样(因为每个平面就是代表每个通道) out_linesize与返回值可能有'通道数'倍 关系
	// 如果format是packet类型 只有一个平面 那么左右通道都放在一个平面 out_linesize与返回值应该一样
	mDecodedFrameSize =av_samples_get_buffer_size(&out_linesize,
												  mpAudCtx->channels,
												  mpAudCtx->frame_size,
												  AV_SAMPLE_FMT_S16 ,
												  1); // 1 = no alignment 每个平面不需要对齐


	TLOGD("目标:每个通道/平面字节数 %d, 解码后每帧字节数(含通道) %d 每帧样本数 %d 格式%s(每样本字节数%d)" ,
		  out_linesize,
		  mDecodedFrameSize,
		  mpAudCtx->frame_size,
		  av_get_sample_fmt_name(AV_SAMPLE_FMT_S16),
		  av_get_bytes_per_sample(AV_SAMPLE_FMT_S16)
		/*
		 * 帧大小 1024(样本数/每通道)
		 * 帧时长 23 ms
		 * 帧大小 8192(字节/所有通道 当前format是fltp 每样本字节数 4)
		 * 通道数 2
		 * ===>
		 * 目标:每个通道/平面字节数 4096, 解码后每帧字节数(含通道) 4096 每帧样本数 1024 格式s16(每样本字节数2)
		 * 源:每个通道/平面字节数 4096, 每帧字节数(含通道) 8192 每帧样本数 1024 格式fltp(每样本字节数4)
		 *
		 * ===>
		 * 1024 * 2(通道) * 2(S16 2个字节) = 4096 字节 (S16是packet类型 只有一个平面 所有out_linesize与返回值一样)
		 */
		// TODO 网络流 可能每帧样本数 不确定 ??? AVPacket不确定包含多少帧 但是解码输出AVFrame.nb_samples会说明当前输出帧含有的样本数目
		// TODO 网络流 可能需要延后 BufferManager的创建
	);

	int bufferSize =av_samples_get_buffer_size(&out_linesize,
												  mpAudCtx->channels,
												  mpAudCtx->frame_size,
												  mpAudCtx->sample_fmt ,
												  1);
	TLOGD("源:每个通道/平面字节数 %d, 每帧字节数(含通道) %d 每帧样本数 %d 格式%s(每样本字节数%d)" ,
		  out_linesize,
		  bufferSize,
		  mpAudCtx->frame_size,
		  av_get_sample_fmt_name((AVSampleFormat)mpAudCtx->sample_fmt),
		  av_get_bytes_per_sample((AVSampleFormat)mpAudCtx->sample_fmt) );

	mBufMgr = new BufferManager(mDecodedFrameSize ,  30 );

#ifdef SAVE_DECODE_TO_FILE
	mSaveFile = new SaveFile("/mnt/sdcard/decode.pcm");
#endif

	prctl(PR_SET_NAME, oldName);

	return true ;
}

void AACSWDecoder::enqloop(){

	int ret = 0 ;
	bool end = false ;

	while( ! mStop && ! end  ){
		sp<MyPacket> mypkt = NULL ;
		AVPacket* packet = NULL;

		AutoMutex l(mEnqMux);
		if( ! mPktQueue.empty()   )
		{
			mypkt = mPktQueue.front();
			mPktQueue.pop_front();
			mEnqSrcCnd->signal();
		}else{
			if(mStop)break;
			//TLOGD("Loop:wait for aac avpacket ");
			mEnqSikCnd->wait(mEnqMux);
			continue;
		}

		if( mypkt.get() == NULL){
			TLOGW("End Of file, mark end");
			end = true ;
		}else{
			packet = mypkt->packet();
		}



	TRY_AGAIN:
		// ret  = avcodec_decode_audio4(mpAudCtx, frame, &got_frame, packet );
		// if( got_frame > 0 ) {
		{
			AutoMutex _l(mSndRcvMux);
			if(end){
				TLOGW("End Of file, try send Empty Packet to Decoder");
				ret = avcodec_send_packet(mpAudCtx,NULL);
			}else{
				ret = avcodec_send_packet(mpAudCtx,packet);
			}
		}
		switch(ret){
			case 0 :{// success avcodec_send_packet
				if(end){
					TLOGW("End Of file, send Empty Packet to Decoder done");
					break;
				}
				TLOGD(">[dts %ld pts %ld] %02x %02x %02x %02x %02x" ,
					  packet->dts,  packet->pts,
					  (packet->data)?packet->data[0]:0xFF,
					  (packet->data)?packet->data[1]:0xFF,
					  (packet->data)?packet->data[2]:0xFF,
					  (packet->data)?packet->data[3]:0xFF,
					  (packet->data)?packet->data[4]:0xFF
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
				TLOGW("enqloop 解码器完全清空(flushed).\n");
				if(end){
					TLOGW("End Of file, send Empty Packet to Decoder done");
				}
			}break;
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
	}
}

void AACSWDecoder::deqloop(){

	AVFrame *pFrame = av_frame_alloc();
	int ret = 0 ;
	bool end = false ;

	while( !mStop & !end ) {
		{
			AutoMutex _l(mSndRcvMux);
			ret = avcodec_receive_frame(mpAudCtx, pFrame); // non-block
		}
		switch (ret) {
			case 0:{//成功
				if (pFrame->pts == AV_NOPTS_VALUE) {
					pFrame->pts = av_frame_get_best_effort_timestamp(pFrame);
				}

				TLOGD("<[pts %" PRId64 " pkt_dts %" PRId64 " pkt_pts %" PRId64 "]"
									  "目前解码帧数: %d "
									  "四个平面 %p[%d] %p[%d] %p[%d] %p[%d] "
									  "当前帧含有样本数(每通道): %d ",
					  pFrame->pts, pFrame->pkt_dts,
					  pFrame->pkt_pts, //AAC没有双向的预测pts和dts 可以看成是一个顺序的，因此可选任何一个
					  mpAudCtx->frame_number,   // frame_number 到目前为止 解码器已返回的总帧数
					  (void *) pFrame->data[0], pFrame->linesize[0],
					  (void *) pFrame->data[1], pFrame->linesize[1],
					  (void *) pFrame->data[2], pFrame->linesize[2],
					  (void *) pFrame->data[3], pFrame->linesize[3],
				/*
				 * 数据源:
				 * 帧大小 1024(样本数/每通道)
				 * 帧时长 23 ms
				 * 帧大小 8192(字节/所有通道 当前format是fltp 每样本字节数 4)
				 * 通道数 2
				 *
				 * 四个平面 0x7f89a1b000[8192] 0x7f89a1d000[0] 0x0[0] 0x0[0] 当前帧含有样本数(每通道): 1024
				 * len = 1024 resampled_data_size = 4096  mDecodedFrameSize = 4096
				 *
				 * 4096 = 1024(len 每通道样本数) * 2 (通道数) * 2(每样本字节数 S16/packet/单一平面)
				 */
				// 对于packet类型 只有data[0]和linesize[0] 8192 = 1024(每个通道一个帧含有多少个sample) * 2(个通道) * 4(sizeof(Float))
				// 对于planar类型 data和linesize数组有效个数==通道数 每个平面/通道的字节数应该相等 linesize[0]=linesize[1]=1024(每个通道一个帧含有多少个sample) * 4(sizeof(Float))
				// 实际发现 多个平面 也只有 lineSize[0] 但是 data[0] data[1]都有指针  lineSize[0]/2 才是每个平面的字节数
					  	  pFrame->nb_samples         // 网络流可能不能确定一个帧包含多少个样本数 解码器会告诉我们这个
				);

				sp<Buffer> buf = mBufMgr->pop();
				if (mpSwrCtx == NULL) {
					/*
                     * 在使用ffmpeg解码aac的时候，如果使用avcodec_decode_audio4函数解码
                     * 那么解码出来的会是 AV_SAMPLE_FMT_FLTP 格式的数据( float, 4字节 , planar)
                     * 如希望得到16bit的数据（如AV_SAMPLE_FMT_S16P数据,2个字节），那么需要转换
                     *
                     * 若sample是 AV_SAMPLE_FMT_FLTP -> sample会是 float 格式，且值域为 [-1.0, 1.0]
                     * 若sample是 AV_SAMPLE_FMT_S16  -> sample会是 int16 格式，且值域为 [-32767, +32767]
                     *
                     *
                     * 早期ffmpeg编码音频，
                     * 输入数据一般都是S16格式，
                     * 解码输出一般也是S16，
                     * 也就是说PCM数据是存储在连续的buffer中
                     * 对一个双声道（左右）音频来说，存储格式可能就为LRLRLR.........
                     *
                     * ffmpeg升级到2.1后，同样对双声道音频PCM数据，以S16P为例，存储就可能是
                     * plane 0: LLLLLLLLLLLLLLLLLLLLLLLLLL...
                     * plane 1: RRRRRRRRRRRRRRRRRRRRRRRRRR...
                     * 而不再是以前的连续buffer
                     * 而AAC编码依旧使用 AV_SAMPLE_FMT_S16格式
                     */
					mpSwrCtx = swr_alloc();
					mpSwrCtx = swr_alloc_set_opts(mpSwrCtx, // 输出数据格式
												  mpAudCtx->channel_layout, // 通道数
												  AV_SAMPLE_FMT_S16,        // 必须转换成16bit,AudioTrack目前只支持S16
												  mpAudCtx->sample_rate,   //  采样率
												  mpAudCtx->channel_layout, // 输入数据格式
												  mpAudCtx->sample_fmt,
												  mpAudCtx->sample_rate,
												  0,
												  NULL);
					swr_init(mpSwrCtx);
				}

				uint8_t *lineaddr[] = {buf->data()}; // 转成S16(packet) 所以只有一个平面
				/*
				 * 样本数:(与通道无关)
				 * swr_convert
				 * 		返回 每个通道的样本数
				 *
				 * 计算每帧率返回的字节大小
				 * 		= 通道的样本数(swr_convert返回值) * 通道数(channel_layout) * 2字节(AV_SAMPLE_FMT_S16)
				 *
				 */
				int32_t len = swr_convert(mpSwrCtx, lineaddr, mDecodedFrameSize,
										  (const uint8_t **) pFrame->data, pFrame->nb_samples);
				if (len > 0) {
					//每声道采样数 x 声道数 x 每个采样字节数
					int resampled_data_size =
							len * mpAudCtx->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
					TLOGD("len = %d resampled_data_size = %d  mDecodedFrameSize = %d ",
						  len, // 每个通道/平面的样本数
						  resampled_data_size,
						  mDecodedFrameSize
					);
					buf->size() = mDecodedFrameSize;
					buf->pts() = (int64_t) (pFrame->pts * mTimeBase * 1000); // ms

#ifdef SAVE_DECODE_TO_FILE
					mSaveFile->save(  buf->data()  , buf->size());
#endif
					// TODO 给到渲染线程
					//mpRender->renderAudio( buf ) ;
				} else {
					TLOGE("deqloop swr_convert error \n");
					// TODO 给Player发送错误事件  Player切换到错误状态 并且反馈给应用层

				}
				av_frame_unref(pFrame);
			}break;
			case AVERROR_EOF:{
				// TODO 告诉Player文件已经解码完毕 可能等待渲染结束
				TLOGW("deqloop 解码器完全清空, 没有更多帧输出.\n");
				// TODO 告诉渲染线程文件已经结束
				// mRender->renderAudio(NULL) ;
				end = true ;
			}break;
			case AVERROR(EAGAIN):{
				// TODO 目前只是休眠
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

void* AACSWDecoder::enqThreadEntry(void *arg)
{
	prctl(PR_SET_NAME,"AACEnqTh");
	AACSWDecoder* decoder = (AACSWDecoder*)arg;
	decoder->enqloop();

	return NULL;
}


void* AACSWDecoder::deqThreadEntry(void *arg) {
	prctl(PR_SET_NAME,"AACDeqTh");
	AACSWDecoder* decoder = (AACSWDecoder*)arg;
	decoder->deqloop();
	return NULL;
}


void AACSWDecoder::start(  )
{
	int ret = 0 ;
	ret = ::pthread_create(&mEnqThID, NULL, AACSWDecoder::enqThreadEntry, (void *) this);
	if(ret < 0){
		TLOGE("create Decode Thread ERROR %d %d %s" , ret ,errno, strerror(errno));
	}

	ret = ::pthread_create(&mDeqThID, NULL, AACSWDecoder::deqThreadEntry, (void *) this);
	if(ret < 0){
		TLOGE("Create Decode Dequeue Thread ERROR %d %d %s" , ret ,errno, strerror(errno));
		assert(ret >= 0 );
	}
}

void AACSWDecoder::stop()
{
	mStop = true ;
	if(mEnqThID != -1) {
		{
			AutoMutex l(mEnqMux);
			mEnqSrcCnd->signal();
			mEnqSikCnd->signal();
		}
		pthread_join(mEnqThID, NULL);
		clearupPacketQueue();
		TLOGD("AACSWDecoder EnqTh stop done");
	}else{
		TLOGD("Decode EnqTh not start yet ");
	}

	if(mDeqThID != -1) {
		// 注意  dequeue线程可能挂在render队列上 所以要先stop Render
		pthread_join(mDeqThID, NULL);
		TLOGD("AACSWDecoder DeqTh stop done");
	}else{
		TLOGD("Decode DeqTh not start yet ");
	}
	TLOGW("AACSWDecoder stop done");
}


bool AACSWDecoder::put(sp<MyPacket> packet , bool wait ){
	while(!mStop){
		AutoMutex l(mEnqMux);
		if( mPktQueue.size() == MAX_PACKET_QUEUE_SIZE ){
			TLOGW("too much video AVPacket wait!");
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



void AACSWDecoder::clearupPacketQueue()
{
	AutoMutex l(mEnqMux);
	std::list<sp<MyPacket>>::iterator it = mPktQueue.begin();
	TLOGI("pending mPktQueue size %lu ", mPktQueue.size() );
	while(it != mPktQueue.end()) {
		mPktQueue.erase(it++);
	}
	TLOGI("clean up AVPacket Done!");
}


AACSWDecoder::~AACSWDecoder(){

	if(mEnqMux!=NULL) 	 { delete mEnqMux;		mEnqMux = NULL ;}
	if(mEnqSrcCnd!=NULL) { delete mEnqSrcCnd;	mEnqSrcCnd = NULL; }
	if(mEnqSikCnd!=NULL) { delete mEnqSikCnd;	mEnqSikCnd = NULL; }

	if( mpAudCtx != NULL){
		avcodec_free_context(&mpAudCtx); mpAudCtx = NULL;
	}
	if( mpSwrCtx != NULL ){
		swr_free(&mpSwrCtx); mpSwrCtx = NULL;
	}
	TLOGT("~AACSWDecoder");
}

