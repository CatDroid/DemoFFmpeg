/*
 * LocalFileDemuxer.cpp
 *
 *  Created on: 2016年12月10日
 *      Author: hanlon
 */

#include "LocalFileDemuxer.h"

#define LOG_TAG "LocalFileDemuxer"
#include "jni_common.h"
#include "ffmpeg_common.h"
#include "project_utils.h"

#include <netinet/in.h>

LocalFileDemuxer::LocalFileDemuxer(const char * file_path)
				:mAvFmtCtx(NULL),mReadThread(-1),mVstream(-1),mAstream(-1),
				 mLoop(true),mEof(false),mAudioSinker(NULL),mVideoSinker(NULL)
{

	mPm = new PacketManager( 20 );
	av_register_all(); 	// 不加 av_register_all avformat_open_input 会导致 Invalid data found when processing input
						// 不加  android.permission.READ_EXTERNAL_STORAGE 会导致 Permission denied

	ALOGD("open file %s " , file_path );
	// 	告诉 libavformat 去自动探测文件格式 并且 使用 默认的缓冲区大小

	/*
	 * AVFormatContext **ps 参数需要初始化为 NULL 由avformat_open_input内部分配
	 * 如果不初始化AVFormatContext* mAvFmtCtx为NULL 会导致 SIGSEGV
	 */
	int ret = avformat_open_input(&mAvFmtCtx, file_path, NULL, NULL);
	if( ret < 0 ){
		ffmpeg_strerror( ret , "open file error ");
		return ;
	}

	if( ret = avformat_find_stream_info(mAvFmtCtx, 0), ret < 0 ){
		ffmpeg_strerror(  ret  , "find stream info error ");
	}
	//ffmpeg_strerror(-13);

	for (int i = 0; i <  mAvFmtCtx->nb_streams; i++) {
		AVStream *st =  mAvFmtCtx->streams[i];
		enum AVMediaType type = st->codecpar->codec_type;
		if( type == AVMEDIA_TYPE_AUDIO){
			mAstream  = i ;
			ALOGI("Found Audio Stream at %d " , mAstream);
		}else if( type == AVMEDIA_TYPE_VIDEO){
			mVstream = i ;
			ALOGI("Found Video Stream at %d " , mVstream);
		}else {
			ALOGI("not video or audio stream : %d " , type );
		}
	}

	if( mVstream != -1 )
	{	// 获取视频流信息: 宽 高 比特率 帧率 视频时长 帧数目 时基(timebase) 解码器特定参数(sps pps)

 		AVCodecParameters * para = mAvFmtCtx->streams[mVstream]->codecpar;

		AVCodecID codecID = para->codec_id ;
		if( codecID == AV_CODEC_ID_H264 ) {
			ALOGD("video stream is H264");
		}else{
			ALOGE("video stream is unknown codecID %d " , codecID);
		}

		AVStream* pStream = mAvFmtCtx->streams[mVstream];
		AVPixelFormat sample_fmt = (AVPixelFormat)para->format; // for video is AVPixelFormat

		AVRational radional =  av_guess_frame_rate(mAvFmtCtx, mAvFmtCtx->streams[mVstream], NULL);
		double fps = av_q2d(radional);
		mVTimebase = av_q2d(pStream->time_base); // 1 / 90,000


		ALOGD("file duration %d us  %d ms "  ,mAvFmtCtx->duration , mAvFmtCtx->duration/1000 );
		ALOGD("video stream (%d,%d), video bitrate=%lld,  fps=%.2f, "
				"sample_format %d (maybe is %s ),"
				"base=%d/%d, nb_frames=%lld, duration=%lld(in in stream time base %f) "
				"duration=%.4f(second )",
				para->width,
				para->height,
				para->bit_rate,
				fps,
				sample_fmt, (sample_fmt==AV_PIX_FMT_YUV420P?"yuv420":"unknown") ,
				pStream->time_base.num, pStream->time_base.den,
				pStream->nb_frames, 	pStream->duration,
			    mVTimebase,
				pStream->duration * mVTimebase );

		uint8_t *src = para->extradata + 5;
		int extradata_size = para->extradata_size ;
		ALOGD("video extradata_size = %d ", extradata_size );
		DUMP_BUFFER_IN_HEX("video csd:",para->extradata, para->extradata_size);

		{//sps
			src += 1;
			int len = ntohs(*(uint16_t*)src);
			src += 2;
			setupVideoSpec(false, true, src, len); src += len;
			DUMP_BUFFER_IN_HEX("sps:",(uint8_t*)mSPS.c_str(),mSPS.size());
		}
		{//pps
			src += 1;
			int len = ntohs(*(uint16_t*)src);
			src += 2;
			setupVideoSpec(true, true , src, len);
			DUMP_BUFFER_IN_HEX("pps:",(uint8_t*)mPPS.c_str(),mPPS.size());
		}
	}
	if( mAstream != -1 )
	{// 获取音频流信息: 采样率 位宽 通道数  音频时长 帧数目 时基(timebase) 解码器特定参数(esds)

		AVCodecParameters * para = mAvFmtCtx->streams[mAstream]->codecpar;

		AVCodecID codecID = para->codec_id ;
		if( codecID == AV_CODEC_ID_AAC ) {
			ALOGD("audio stream is AAC");
		}else{
			ALOGE("audio stream is unknown codecID %d " , codecID);
		}

		AVStream* pStream = mAvFmtCtx->streams[mAstream];
		mATimebase = av_q2d(pStream->time_base);

		AVSampleFormat sample_fmt = (AVSampleFormat)para->format; // for audio is AVSampleFormat

		char  layout_string[32];
		av_get_channel_layout_string( layout_string,sizeof(layout_string),para->channels,para->channel_layout);
		/*
		 * 帧大小和时长
		 *		frame_size 每个通道一个音频帧的样本数
		 *		同AVCodecParameters 	是AVFormat容器已经获取的参数
		 *
		 * 		frame_size*2channel*2byte(Bitwitdh) = 一个frame总的字节大小(双通道)
		 * 		frame_size / 44100Hz = 一帧播放的时长
		 * 		e.g 一个音频帧每个通道的样本数1024
		 * 			一个音频帧 每个通道的 样本数 1024
		 * 			也就是一个音频帧(AAC) 可以播放 1024* (1/sample_rate) = 0.023 ms
		 * 			相当于 音频帧的周期 是 frame_size/sample_rate (fps 每隔 这么多时间 要取一个AAC帧 去解码播放)
		 * 			nb_frames AAC数目  2768 * ~23ms(每个AAC可以播放时长) = 63,664ms
		 *
		 * 格式:
		 * 		输入数据一般都是S16格式，解码输出一般也是S16，
		 * 		也就是说PCM数据是存储在连续的buffer中，对一个双声道（左右）音频来说，存储格式可能就为
		 * 		LRLRLR.........
		 * 		对双声道音频PCM数据，以S16P为例，存储就可能是
		 * 		plane 0: LLLLLLLLLLLLLLLLLLLLLLLLLL...
		 * 		plane 1: RRRRRRRRRRRRRRRRRRRRRRRRRR...
		 * 		而不再是以前的连续buffer,如mp3编码就明确规定了只使用平面格式的数据
		 *
		 * 		新版ffmpeg解码音频输出的格式可能不满足S16，
		 * 		如AAC解码后得到的是FLT（浮点型），AC3解码是FLTP（带平面）等，
		 * 		需要根据具体的情况决定是否需要 swr_convert
		 *
		 * 数据:
		 * 		对于浮点格式，其值在[-1.0,1.0]之间，任何在该区间之外的值都超过了最大音量的范围
		 * 		若 sample 是 AV_SAMPLE_FMT_FLTP，則 sample 會是 float 格式，且值域为 [-1.0, 1.0]
		 * 		若 sample 是 AV_SAMPLE_FMT_S16， 則 sample 會是 int16 格式，且值域为 [-32767, +32767]
		 * 		音频的采样格式分为平面（planar）和打包（packed）两种类型，
		 * 		在枚举值中上半部分是packed类型，后面（有P后缀的）是planar类型
		 * 		对于planar格式的，每一个通道的值都有一个单独的plane，所有的plane必须有相同的大小；
		 * 		对于packed类型，所有的数据在同一个数据平面中，不同通道的数据交叉保存
		 *
		 */
		ALOGD("Audio Stream Param:\ncodecid=%d(see AVCodecID)\n"
					  "帧大小 %d(样本数/每通道)\n"
					  "帧时长 %d ms \n"
					  "帧大小 %d(字节/所有通道)\n"
					  "PCM格式 %d(%s %s see AVSampleFormat)\n"
					  "通道数 %d\n"
					  "通道存储顺序 %s/%d ( %s %d , %s %d see channel_layout.h)\n"
					  "采样率 %d\n"
					  "每样本大小 %d字节\n"
					  "每样本大小 %dbits\n"
			          "时间基准 %d/%d\n"
		              "总帧数 %lld\n"
					  "时长 %lld(单位为时间基准 %f)\n"
					  "时长 %.4f(秒 )\n"
			          "时长 %.4f(秒 根据帧数和帧时长计算)\n",
			  para->codec_id ,
			  para->frame_size,
			  para->frame_size * 1000 / para->sample_rate ,
			  para->frame_size * para->channels * av_get_bytes_per_sample((AVSampleFormat)para->format),
			  (AVSampleFormat)para->format, // AV_SAMPLE_FMT_FLTP,  < float, planar 16bit不是AV_SAMPLE_FMT_S16
			  av_get_sample_fmt_name((AVSampleFormat)para->format),
			  av_sample_fmt_is_planar((AVSampleFormat)para->format)?"平面(plannar)":"非平面(packed)",
			  para->channels,
			  layout_string,
			  para->channel_layout,
			  av_get_channel_name(AV_CH_FRONT_LEFT), av_get_channel_layout_channel_index(AV_CH_LAYOUT_STEREO,AV_CH_FRONT_LEFT),
			  av_get_channel_name(AV_CH_FRONT_RIGHT), av_get_channel_layout_channel_index(AV_CH_LAYOUT_STEREO,AV_CH_FRONT_RIGHT),
			  para->sample_rate,
			  av_get_bytes_per_sample((AVSampleFormat)para->format),
			  para->bits_per_raw_sample ,
			  pStream->time_base.num, pStream->time_base.den,
			  pStream->nb_frames, 	pStream->duration,
			  mATimebase,
			  pStream->duration * mATimebase ,
			  pStream->nb_frames * para->frame_size * 1.0f / para->sample_rate  );

		//esds
		uint8_t *src = para->extradata ;
		int extradata_size = para->extradata_size ;
		ALOGD("audio extradata_size = %d ", extradata_size );
		DUMP_BUFFER_IN_HEX("audio csd:" ,para->extradata, para->extradata_size);

		setupAudioSpec( true , src, extradata_size);
		DUMP_BUFFER_IN_HEX("esds:",(uint8_t*)mESDS.c_str(),mESDS.size());


	}

	ALOGD("avformat_open done !");

}
/*
	3gp和mp4 esds pps sps
 	AAC的私有数据保存在esds的0x05标签的数据 ， 结构为 05 + 长度 + 内容
	将长度赋值给 extradatasize		长度的计算函数在ffmpeg中的static int mp4_read_descr_len(ByteIOContext *pb)
	将内容赋值给 extradata
	AVC/H264的extradata和extradata信息在avcc atom中
	将avcc atom去掉type和长度（8个字节）后的长度赋予extradatasize
	内容赋值给extradata
 */

void LocalFileDemuxer::play()
{
	int ret = ::pthread_create(&mReadThread, NULL, LocalFileDemuxer::extractThread, (void*)this  );
}

void LocalFileDemuxer::stop()
{
	if( mReadThread != 0 ){
		mLoop = false ;
		::pthread_join( mReadThread  , NULL);
		mReadThread = 0 ;
	}
	ALOGD("LocalFileDemuxer stop done");
}
LocalFileDemuxer::~LocalFileDemuxer()
{
	ALOGD("~LocalFileDemuxer ");
}

AVCodecParameters* LocalFileDemuxer::getVideoCodecPara()
{
	if( mVstream != -1 ){
		return mAvFmtCtx->streams[mVstream]->codecpar;
	}
	return NULL;
}

AVCodecParameters* LocalFileDemuxer::getAudioCodecPara()
{
	if( mAstream != -1 ){
		return mAvFmtCtx->streams[mAstream]->codecpar;
	}
	return NULL;
}




void LocalFileDemuxer::setupVideoSpec(bool is_pps , bool addLeaderCode , unsigned char *data, int size)
{
	if( ! is_pps  )
	{
		mSPS.resize( size + (addLeaderCode ? 4 : 0 ) );
		if( addLeaderCode ) memcpy((void*)mSPS.c_str(), "\x00\x00\x00\x01", 4);
		memcpy((void*)(mSPS.c_str() + 4), data, size);
	}
	else
	{
		mPPS.resize( size + (addLeaderCode ? 4 : 0 ) );
		if( addLeaderCode ) memcpy((void*)mPPS.c_str(), "\x00\x00\x00\x01", 4);
		memcpy((void*)(mPPS.c_str() + 4), data, size);
	}
}

void LocalFileDemuxer::setupAudioSpec(bool addLeaderCode , unsigned char *data, int size)
{
	mESDS.resize( size + (addLeaderCode ? 4 : 0 ) );
	if( addLeaderCode ) memcpy((void*)mESDS.c_str(), "\x00\x00\x00\x01", 4);
	memcpy((uint8_t*)mESDS.c_str() + 4, data, size);
}


void LocalFileDemuxer::setAudioSinker(DeMuxerSinkBase* audioSinker)
{
	if( mAudioSinker != NULL ) ALOGW("multi called setAudioSinker ");
	mAudioSinker = audioSinker ;
}
void LocalFileDemuxer::setVideoSinker(DeMuxerSinkBase* videoSinker)
{
	if( mVideoSinker != NULL ) ALOGW("multi called setVideoSinker ");
	mVideoSinker = videoSinker ;
}


void LocalFileDemuxer::loop()
{
	sp<MyPacket> pkt = NULL;
    int64_t stream_start_time;
    int pkt_in_play_range = 0;
    int64_t pkt_ts;
    static int64_t start_time = AV_NOPTS_VALUE;  // 定义开始播放的时间和结束时间
    int64_t duration = AV_NOPTS_VALUE  ;

    /**
     * Decoding: pts of the first frame of the stream in presentation order, in stream time base.
     * Only set this if you are absolutely 100% sure that the value you set
     * it to really is the pts of the first frame.
     * This may be undefined (AV_NOPTS_VALUE).
     * @note The ASF header does NOT contain a correct start_time the ASF
     * demuxer must NOT set this.
     */
    int64_t video_start_time = mAvFmtCtx->streams[mVstream]->start_time ;
    int64_t audio_start_time =  mAvFmtCtx->streams[mAstream]->start_time ;
    double video_timebase = av_q2d(mAvFmtCtx->streams[mVstream]->time_base) ;
    double audio_timebase = av_q2d(mAvFmtCtx->streams[mAstream]->time_base) ;

    ALOGD("video stream start %lld(time base %f) %f(sec) , ",
    		video_start_time , video_timebase ,( video_start_time == AV_NOPTS_VALUE ? -1 : video_start_time * video_timebase )
    		);
    ALOGD("audio stream start %lld(time base %f)  %f(sec)" ,
    		audio_start_time , audio_timebase , (audio_start_time == AV_NOPTS_VALUE ? -1 : audio_start_time * audio_timebase )
    		);

	for (;;) {
		if (!mLoop) break;

		pkt = mPm->pop();
		int ret = av_read_frame(mAvFmtCtx, pkt->packet());
		if (ret < 0) {
			if ((ret == AVERROR_EOF || avio_feof(mAvFmtCtx->pb)) && ! mEof ) {
				mEof = 1;
			}
			if (mAvFmtCtx->pb && mAvFmtCtx->pb->error){
				ALOGD("ERROR mAvFmtCtx ERROR ");
				ffmpeg_strerror(mAvFmtCtx->pb->error);
				break;
			}
		} else {
			mEof = false;
		}
		/* check if packet is in play range specified by user, then queue, otherwise discard */
		stream_start_time = mAvFmtCtx->streams[pkt->packet()->stream_index]->start_time; // 时间单位是 该流的 time base

		if (pkt->packet()->stream_index == mAstream  ) {
			ALOGD("audio dts = %ld , pts=%f,dts=%f,duration %f" ,
				  pkt->packet()->dts ,
				  pkt->packet()->pts * audio_timebase ,
				  pkt->packet()->dts * audio_timebase ,
				  pkt->packet()->duration* audio_timebase  );
			//pkt->pts = pkt->pts * audio_timebase * 1000  ; // 强制改变时间戳 ms
			mAudioSinker->put(pkt);

		} else if (pkt->packet()->stream_index == mVstream ) {
			/*
			 * 第一帧IDR 65帧 dts是负数 pts是0
			 * video dts = -1502 , pts=0.000000,dts=-0.016689,duration 0.016678
			 * video dts = 0 , pts=0.016689,dts=0.000000,duration 0.016678
			 * */
			ALOGD("video dts = %ld , pts=%f,dts=%f,duration %f" ,
				  pkt->packet()->dts ,
				  pkt->packet()->pts * video_timebase ,
				  pkt->packet()->dts * video_timebase ,
				  pkt->packet()->duration* video_timebase );
			//pkt->dts = pkt->dts * video_timebase * 1000 ;
			//pkt->pts  = pkt->pts * video_timebase * 1000  ; // 强制改变时间戳 ms
			//mVideoSinker->put(pkt);

		} else {
			ALOGD("discard avpacket");
		}
		pkt = NULL;
		//usleep(10*1000);
	}

}

void* LocalFileDemuxer::extractThread( void* arg)
{
	LocalFileDemuxer* player = (LocalFileDemuxer *) arg;
	//JNI_ATTACH_JVM_WITH_NAME( );
	ALOGD("Extractor Thread Enter");
	player->loop();
	ALOGD("Extractor Thread Exit");
	//JNI_DETACH_JVM( )
	return NULL;
}


