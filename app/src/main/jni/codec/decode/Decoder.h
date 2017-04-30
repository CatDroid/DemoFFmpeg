//
// Created by Hanloong Ho on 17-4-9.
//

#ifndef DEMO_FFMPEG_AS_DECODER_H
#define DEMO_FFMPEG_AS_DECODER_H


#include "Render.h"
#include "MyPacket.h"

class Decoder {

protected:
    Render* mRender ;

public:
    Decoder():mRender(NULL){}

    /*
     * 尝试初始化解码器
     * @param para 解码器参数 对于网络播放来说,要从SDP中获取相关参数填充,由demuxer生成和保管,init返回后不引用*para
     * @param timebase 时间基准, 对于本地播放来说,AVFormatContext.Stream[].time_base中
     * @return true 解码器初始化成功 ; false 初始化失败,可以尝试其他解码器
     */
    virtual bool init(const AVCodecParameters* para, double timebase) = 0 ;

    /*
     * 设置解码数据的输出目标
     *
     * @param render 输出到指定的Render对象
     */
    virtual void setRender(Render* render ) {mRender = render; }

    /*
     * 启动线程
     * 1.启动一个enqueue线程和一个dequeue线程
     * 2.enqueue线程从put进来的 AVPacket包队列 中获取AVPacket放进编码器 ---Feeding loop
     * 3.dequeue线程不断从解码器获取数据 推送给Render队列               ---Draining enqloop
     * 4.推送给Render可能会阻塞,
     *      然后导致解码器内部满了,不能再推送数据,
     *      最终deMuxer不能推送新的AVPacket,
     *      对于本地播放来说,导致demuxer停止从文件中读取数据加载到AVPacket(内存)
     *      对于网络播放来说,导致丢帧,需要重新同步
     */
    virtual void start() = 0 ;
    /*
     * 设置停止标记
     * */
    virtual void stop() = 0 ;
    /*
     * 推送一个待解码的AVPacket
     *
     * @param wait
     *      true 等待解码器enqueue队列有空闲
     *      false 如果解码器队列满的话,不等待,并立刻返回false(直播可能需要,用于丢帧和同步关键帧率)
     * @param packet
     *      需要解码的包,返回时候MyPacket packet应该已使用完毕,并还给PacketManager
     */
    virtual bool put(sp<MyPacket> packet, bool wait) = 0 ;

    /*
     * seek之后更新
     */
    virtual void flush( ) = 0 ;

    /*
     * 析构 如果已经start的话,
     * 1.唤醒等待在 解码器enqueue队列 的线程(put wait=true的demuxer)
     * 2.join enqueue线程和dequeue线程
     */
    virtual ~Decoder(){};

};


#endif //DEMO_FFMPEG_AS_DECODER_H
