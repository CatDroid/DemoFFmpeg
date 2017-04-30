/*
 * LocalFileDemuxer.cpp
 *
 *  Created on: 2016年12月10日
 *      Author: hanlon
 */

#include "LocalFileDemuxer.h"


#include "ffmpeg_common.h"
#include "project_utils.h"
#include "Decoder.h"

#include <netinet/in.h>
#include <sys/prctl.h>

CLASS_LOG_IMPLEMENT(LocalFileDemuxer,"LocalFileDemuxer");





LocalFileDemuxer::LocalFileDemuxer(Player* player):DeMuxer(player),
			mExtractThID(-1),mStop(false),mPause(false),mEof(false),mParseResult(false),
			mAvFmtCtx(NULL),mVstream(-1),mAstream(-1) ,
			 mVTimebase(-1),mATimebase(-1),mDuration(0)
{
	mPm = new PacketManager( 20 );

	mStartMutex = new Mutex();
	mStartCon = new Condition();
	TLOGD("LocalFileDemuxer");

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

void LocalFileDemuxer::prepareAsyn()
{
	int ret = ::pthread_create(&mExtractThID, NULL, LocalFileDemuxer::exThreadEntry,
							   (void *) this);
	if(ret != 0 ){
		TLOGE("LocalFileDemuxer pthread_create fail %d %d %s " , ret ,errno , strerror(errno));
	}
	assert(ret == 0);
}

void LocalFileDemuxer::play() {
	AutoMutex _l(mStartMutex);
	mPause = false ;
	mStartCon->signal();

}

void LocalFileDemuxer::pause() {
	AutoMutex _l(mStartMutex);
	mPause = true ;
}

void LocalFileDemuxer::seekTo(int32_t ms) {
	AutoMutex _l(mSeekMutex);

	int idx = mVstream!=-1? mVstream : mAstream  ; // 如果有时间的话 以视频seek 因为H264需要同步到关键帧
	const AVStream *pStream = mAvFmtCtx->streams[idx]; // a * b / c    ms / (1/90000)
	const int64_t skips = av_rescale(ms, pStream->time_base.den, pStream->time_base.num);
	/*
	 * #define AVSEEK_FLAG_BYTE     2 ///< seeking based on position in bytes
	 * #define AVSEEK_FLAG_FRAME    8 ///< seeking based on frame number  timestamp是帧号为单位
	 *
	 * 如果没有上述标记 就是以AV_TIME_BASE为单位 1,000,000 (也就是us).
	 *
	 * #define AVSEEK_FLAG_BACKWARD 1 ///< seek backward
	 * #define AVSEEK_FLAG_ANY      4 ///< seek to any frame, even non-keyframes 不是seek到关键帧
	 *
	 */
	TLOGW("try to seekTo %d by %s stream", ms ,mAvFmtCtx->streams[idx]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO?"video":"audio" );
	int ret = avformat_seek_file(mAvFmtCtx, idx, INT64_MIN, skips/1000, INT64_MAX, 0 );
	if(ret != 0) {
		mSeekResult = false ;
		mPlayer->seek_complete();
	}else{
		TLOGW("avformat_seek_file done !");
		if(mVDecoder!=NULL){
			TLOGW("flush video decoder");
			mVDecoder->flush();
		}
		if(mADecoder!=NULL){
			TLOGW("flush audio decoder");
			mADecoder->flush();
		}
		mSeekResult = true ;
		mPlayer->seek_complete();
	}

}


void LocalFileDemuxer::stop()
{
	if( mExtractThID != 0 ){
		mStop = true ;
		{ // 针对暂停状态或者还没有开始过情况
			AutoMutex _l(mStartMutex);
			mStartCon->signal();
		}
		::pthread_join( mExtractThID  , NULL);
		mExtractThID = 0 ;
	}
	TLOGD("LocalFileDemuxer stop done");
}
LocalFileDemuxer::~LocalFileDemuxer()
{
	delete mStartMutex  ;
	delete mStartCon ;
	TLOGD("~LocalFileDemuxer");
}

const AVCodecParameters* LocalFileDemuxer::getVideoCodecPara()
{
	if( mVstream != -1 ){
		return mAvFmtCtx->streams[mVstream]->codecpar;
	}
	return NULL;
}

const AVCodecParameters* LocalFileDemuxer::getAudioCodecPara()
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

int32_t LocalFileDemuxer::getDuration(){
	return mDuration;
}

bool LocalFileDemuxer::parseFile() {

	const char* file_path = mUri.c_str() + strlen("file://");// /mnt/sdcard/temp.mp4
	TLOGD("open file %s " , file_path );

	AVDictionary* dic = NULL;
	/*
	 * 注意:
	 * 1. AVInputFormat *fmt == NULL 告诉 libavformat 去自动探测文件格式 并且 使用 默认的缓冲区大小
	 * 2. AVFormatContext **ps 参数需要初始化为 NULL 由avformat_open_input内部分配
	 * 	  如果不初始化AVFormatContext* mAvFmtCtx为NULL 会导致 SIGSEGV
	 */
	int ret = avformat_open_input(&mAvFmtCtx, file_path, NULL, &dic);
	if( ret < 0 ){
		ffmpeg_strerror( ret , "open file error ");
		return false ;
	}

	// DEBUG: 检测avformat_open_input返回的dic 可能返回空的
	AVDictionaryEntry* entry = NULL;
	std::string file_input_dict ;
	while( entry=av_dict_get( dic , "" , entry , 0), entry != NULL ){
		file_input_dict+= entry->key; file_input_dict+= "\t:"; file_input_dict+= entry->value; file_input_dict+="\n";
	}
	TLOGD("avformat_open_input dict %p num %d dic:\n%s", dic , av_dict_count(dic), file_input_dict.c_str());
	av_dict_free(&dic); // 不再需要 AVDictionary !


	if( ret = avformat_find_stream_info(mAvFmtCtx, 0), ret < 0 ){// 填充AVFormatContext的流域
		ffmpeg_strerror(  ret  , "find stream info error ");
		return false ;
	}


	for (int i = 0; i <  mAvFmtCtx->nb_streams; i++) {
		AVStream *st =  mAvFmtCtx->streams[i]; // 解复用 得到 视频和音频流
		enum AVMediaType type = st->codecpar->codec_type;
		if( type == AVMEDIA_TYPE_AUDIO){
			mAstream  = i ;
			TLOGI("Found Audio Stream at %d " , mAstream);

		}else if( type == AVMEDIA_TYPE_VIDEO){
			mVstream = i ;
			TLOGI("Found Video Stream at %d " , mVstream);
		}else {
			TLOGI("not video or audio stream : %d " , type );
		}
		// DEBUG:
		//		打印详细信息
		// 		AVFormatContext 内部关联到 AVInputFormat iformat 或者 AVOutputFormat oformat
		av_dump_format(mAvFmtCtx, i, file_path, 0 /*is_output input(0)*/);
	}

	/*
	 * 注意：
	 * 1. 视频的元数据（metadata）信息可以通过AVDictionary获取。元数据存储在AVDictionaryEntry结构体中
	 * 		在ffmpeg中通过av_dict_get( AVFormatContext.metadata 不是 AVCodecContext )函数获得视频的元数据
	 * 		每一条元数据分为key和value两个属性
	 *
	 * 2. m=av_dict_get(pFormatCtx->metadata,"copyright",NULL,0); // NULL就是不能循环
	 *
	 */
	std::string meta ;
	AVDictionaryEntry *m = NULL;
	while( m=av_dict_get(mAvFmtCtx->metadata,"",m,AV_DICT_IGNORE_SUFFIX), m!=NULL){
		meta+= m->key; meta+= "\t:"; meta+= m->value; meta+="\n";
	}
	TLOGD("AVFormat meta:\n%s", meta.c_str());

	if( mVstream != -1 )
	{	// 获取视频流信息: 宽 高 比特率 帧率 视频时长 帧数目 时基(timebase) 解码器特定参数(sps pps)

		AVCodecParameters * para = mAvFmtCtx->streams[mVstream]->codecpar;
		AVCodecID codecID = para->codec_id ;
		if( codecID == AV_CODEC_ID_H264 ) {
			TLOGD("video stream is H264");
		}else{
			TLOGE("video stream is unknown codecID %d " , codecID);
		}

		AVStream* pStream = mAvFmtCtx->streams[mVstream];
		AVPixelFormat sample_fmt = (AVPixelFormat)para->format; // for video is AVPixelFormat

		/*
		 * time_base.num是分子
		 * time_base.den是分母
		 * av_q2d 就是 把 num/den
		 */
		AVRational radional =  av_guess_frame_rate(mAvFmtCtx, mAvFmtCtx->streams[mVstream], NULL);
		double fps = av_q2d(radional);
		mVTimebase = av_q2d(pStream->time_base); // 1 / 90,000


		TLOGD("文件时长 %" PRId64 " us  %" PRId64 " ms "  ,
			  mAvFmtCtx->duration ,
			  mAvFmtCtx->duration/1000 );

		mDuration = (int32_t) (mAvFmtCtx->duration / 1000);

		TLOGD("视频流(%d,%d):\n"
					  "比特率=%" PRId64 "\n"
					  "帧率=%.2f\n"
					  "像素格式 %d (可能是 %s )\n"
					  "时间基准 %d/%d\n"
					  "该流总帧数 %" PRId64 "\n"
					  "时长 %" PRId64 "(in in stream time base %f)\n"
					  "时长 %.4f(second )\n",
			  para->width,
			  para->height,
			  para->bit_rate,
			  fps,
			  sample_fmt, pixelFormat2str(sample_fmt) ,// 注意:有些片源是YUV422 YUV444等,可能在某些播放器无法播放
			  pStream->time_base.num, pStream->time_base.den,
			  pStream->nb_frames,
			  pStream->duration,  mVTimebase,
			  pStream->duration * mVTimebase );

		// 时长
		if( mDuration < pStream->duration * mVTimebase * 1000 /*ms*/ ){
			mDuration = (int32_t) (pStream->duration * mVTimebase * 1000);
		}

		uint8_t *src = para->extradata + 5;
		int extradata_size = para->extradata_size ;
		TLOGD("video extradata_size = %d ", extradata_size );
		DUMP_BUFFER_IN_HEX("video csd:",para->extradata, para->extradata_size,"LocalFileDemuxer");

		{//sps
			src += 1;
			int len = ntohs(*(uint16_t*)src);
			src += 2;
			setupVideoSpec(false, true, src, len); src += len;
			DUMP_BUFFER_IN_HEX("sps:",(uint8_t*)mSPS.c_str(),mSPS.size(),"LocalFileDemuxer");
		}
		{//pps
			src += 1;
			int len = ntohs(*(uint16_t*)src);
			src += 2;
			setupVideoSpec(true, true , src, len);
			DUMP_BUFFER_IN_HEX("pps:",(uint8_t*)mPPS.c_str(),mPPS.size(),"LocalFileDemuxer");
		}
	}
	if( mAstream != -1 )
	{// 获取音频流信息: 采样率 位宽 通道数  音频时长 帧数目 时基(timebase) 解码器特定参数(esds)

		AVCodecParameters * para = mAvFmtCtx->streams[mAstream]->codecpar;

		AVCodecID codecID = para->codec_id ;
		if( codecID == AV_CODEC_ID_AAC ) {
			TLOGD("audio stream is AAC");
		}else{
			TLOGE("audio stream is unknown codecID %d " , codecID);
		}

		AVStream* pStream = mAvFmtCtx->streams[mAstream];
		mATimebase = av_q2d(pStream->time_base);


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
		TLOGD("音频流:\ncodecid=%d(see AVCodecID)\n"
					  "帧大小 %d(样本数/每通道)\n"
					  "帧时长 %d ms \n"
					  "帧大小 %d(字节/所有通道 当前format是%s 每样本字节数 %d)\n"
					  "PCM格式 %d(%s %s see AVSampleFormat)\n"
					  "通道数 %d\n"
					  "通道存储顺序 %s/%" PRIu64 " ( %s %d , %s %d see channel_layout.h)\n"
					  "采样率 %d\n"
					  "每样本大小 %d字节\n"
					  "每样本大小 %dbits\n"
					  "时间基准 %d/%d\n"
					  "总帧数 %" PRId64 "\n"
					  "时长 %" PRId64 "(单位为时间基准 %f)\n"
					  "时长 %.4f(秒 )\n"
					  "时长 %.4f(秒 根据帧数和帧时长计算)\n",
			  para->codec_id ,
			  para->frame_size,
			  para->frame_size * 1000 / para->sample_rate ,
			  para->frame_size * para->channels * av_get_bytes_per_sample((AVSampleFormat)para->format), av_get_sample_fmt_name((AVSampleFormat)para->format),av_get_bytes_per_sample((AVSampleFormat)para->format),
			  (AVSampleFormat)para->format, // AV_SAMPLE_FMT_FLTP,  < float, planar 16bit不是AV_SAMPLE_FMT_S16
			  av_get_sample_fmt_name((AVSampleFormat)para->format),
			  av_sample_fmt_is_planar((AVSampleFormat)para->format)?"平面(plannar)":"非平面(packed)",
			  para->channels,
			  layout_string, para->channel_layout,
			  av_get_channel_name(AV_CH_FRONT_LEFT), av_get_channel_layout_channel_index(AV_CH_LAYOUT_STEREO,AV_CH_FRONT_LEFT),
			  av_get_channel_name(AV_CH_FRONT_RIGHT), av_get_channel_layout_channel_index(AV_CH_LAYOUT_STEREO,AV_CH_FRONT_RIGHT),
			  para->sample_rate,
			  av_get_bytes_per_sample((AVSampleFormat)para->format),// for audio is AVSampleFormat
			  para->bits_per_raw_sample ,
			  pStream->time_base.num, pStream->time_base.den,
			  pStream->nb_frames, 	pStream->duration,
			  mATimebase,
			  pStream->duration * mATimebase ,
			  pStream->nb_frames * para->frame_size * 1.0f / para->sample_rate  );

		// 时长
		if( mDuration < pStream->duration * mATimebase * 1000 /*ms*/ ){
			mDuration = (int32_t) (pStream->duration * mATimebase * 1000);
		}

		//esds
		uint8_t *src = para->extradata ;
		int extradata_size = para->extradata_size ;
		TLOGD("audio extradata_size = %d ", extradata_size );
		DUMP_BUFFER_IN_HEX("audio csd:" ,para->extradata, para->extradata_size,"LocalFileDemuxer");

		setupAudioSpec( true , src, extradata_size);
		DUMP_BUFFER_IN_HEX("esds:",(uint8_t*)mESDS.c_str(),mESDS.size(),"LocalFileDemuxer");

	}

	TLOGD("avformat_open done !");
	return true ;
}


void LocalFileDemuxer::loop()
{
	sp<MyPacket> pkt = NULL;
    int64_t stream_start_time;
//    int pkt_in_play_range = 0;
//    int64_t pkt_ts;
//    static int64_t start_time = AV_NOPTS_VALUE;  // 定义开始播放的时间和结束时间
//    int64_t duration = AV_NOPTS_VALUE  ;


	mParseResult = parseFile();
	mPlayer->prepare_result();
	if(! mParseResult ){
		TLOGE("parseFile error, enqloop exit");
		return ;
	}else{
		int64_t video_start_time = mAvFmtCtx->streams[mVstream]->start_time ;
		int64_t audio_start_time =  mAvFmtCtx->streams[mAstream]->start_time ;
		double video_timebase = av_q2d(mAvFmtCtx->streams[mVstream]->time_base) ;
		double audio_timebase = av_q2d(mAvFmtCtx->streams[mAstream]->time_base) ;
		TLOGD("video stream start %" PRId64 "(time base %f) %f(sec) , ",
			  video_start_time , video_timebase ,( video_start_time == AV_NOPTS_VALUE ? -1 : video_start_time * video_timebase )
		);
		TLOGD("audio stream start %" PRId64 "(time base %f)  %f(sec)" ,
			  audio_start_time , audio_timebase , (audio_start_time == AV_NOPTS_VALUE ? -1 : audio_start_time * audio_timebase )
		);

		TLOGW("parseFile success , wait for playing");

	}


	{
		AutoMutex _l(mStartMutex);
		mStartCon->wait(mStartMutex);
	}

	for (;;) {
		if (mStop) break;

		if( mPause ) {
			mPlayer->pause_complete();
			AutoMutex _l(mStartMutex);
			mStartCon->wait(mStartMutex);
			continue ;
		}

		pkt = mPm->pop();
		//TLOGD("before read packet %p %d " , pkt->packet()->data, pkt->packet()->size);

		/*
		 * 在从一个视频文件中的包中用例程 av_read_packet()来读取数据时,一个视频帧的信息通常可以包含在几个包里
		 * 而另情况更为复杂的是,实际上两帧之间的边界还可以存在于两个包之间。
		 *
		 * ffmpeg 0.4.9 引入了新的叫做 av_read_frame()的例程,它可以从一个简单的包里返回一个视频帧包含的所有数据
		 * 保证了一个AVPacket是对应一个视频帧率
		 *
		 * 对于视频  返回一个精准的一帧
		 * 对于音频  返回一定整数数量的帧(如果帧大小固定的话，e.g PCM or ADPCM data)
		 * 			或者返回一帧(如果帧大小不固定 e.g MPEG系列audio)
		 */
		int ret = 0 ;
		{
			AutoMutex _l(mSeekMutex);
			ret = av_read_frame(mAvFmtCtx, pkt->packet());
		}
		if (ret < 0) {
			if ((ret == AVERROR_EOF || avio_feof(mAvFmtCtx->pb)) && ! mEof ) {
				TLOGD("End Of File!");
				mEof = true;
				/*
				 * 推送特殊包给到解码线程 --> 推送特殊包给显示线程
				 *
				 * */
				if(mADecoder != NULL) mADecoder->put(NULL,true);
				if(mVDecoder != NULL) mVDecoder->put(NULL,true);
				break;
			}
			if (mAvFmtCtx->pb && mAvFmtCtx->pb->error){
				TLOGD("ERROR mAvFmtCtx ERROR ");
				ffmpeg_strerror(mAvFmtCtx->pb->error);
				break;
			}
		} else {
			mEof = false;
		}
		/* check if packet is in play range specified by user, then queue, otherwise discard */
		stream_start_time = mAvFmtCtx->streams[pkt->packet()->stream_index]->start_time; // 时间单位是 该流的 time base

		// while [ 1 ] ; do adb shell ps -t 32058 ; sleep 1 ; done;
		if (pkt->packet()->stream_index == mAstream  && mADecoder != NULL ) {
			TLOGD("audio dts = %ld , pts=%f,dts=%f,duration %f" ,
				  pkt->packet()->dts ,
				  pkt->packet()->pts * mATimebase ,
				  pkt->packet()->dts * mATimebase ,
				  pkt->packet()->duration* mATimebase  );
			//pkt->pts = pkt->pts * audio_timebase * 1000  ; // 强制改变时间戳 ms
			mADecoder->put(pkt , true );

		} else if (pkt->packet()->stream_index == mVstream && mVDecoder != NULL) {
			/*
			 * 第一帧IDR 65帧 dts是负数 pts是0
			 * video dts = -1502 , pts=0.000000,dts=-0.016689,duration 0.016678
			 * video dts = 0 , pts=0.016689,dts=0.000000,duration 0.016678
			 * */
			TLOGD("video dts = %ld , pts=%f,dts=%f,duration %f" ,
				  pkt->packet()->dts ,
				  pkt->packet()->pts * mVTimebase ,
				  pkt->packet()->dts * mVTimebase ,
				  pkt->packet()->duration* mVTimebase );
			//pkt->dts = pkt->dts * video_timebase * 1000 ;
			//pkt->pts  = pkt->pts * video_timebase * 1000  ; // 强制改变时间戳 ms
			mVDecoder->put(pkt,true);

		} else {
			TLOGD("discard avpacket what stream ? %d " , pkt->packet()->stream_index );
		}
		pkt = NULL;
		//usleep(10*1000);
	}

}


void* LocalFileDemuxer::exThreadEntry(void *arg)
{
	prctl(PR_SET_NAME,"LocalFileDemuxer");
	LocalFileDemuxer* demuxer = (LocalFileDemuxer *) arg;
	LOGD(s_logger,"Extractor Thread Enter");
	demuxer->loop();
	LOGD(s_logger,"Extractor Thread Exit");
	return NULL;
}


