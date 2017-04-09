//
// Created by Hanloong Ho on 17-4-9.
//
#include "DecoderFactory.h"

#include <assert.h>

#include "Log.h"
#include "AACSWDecoder.h"
#include "H264SWDecoder.h"
#define LOG_TAG "DecoderFactory"



Decoder* DecoderFactory::create(const AVCodecParameters* para, double timebase){
    switch(para->codec_id){
        case AV_CODEC_ID_H264:
            return new H264SWDecoder(para , timebase);
        case AV_CODEC_ID_AAC:
            return new AACSWDecoder(para , timebase);
        default:
            LOGE(LOG_TAG, "unsupport format %d ", para->codec_id );
            assert(false);
            break;
    }
    return NULL;
}