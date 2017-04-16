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
        case AV_CODEC_ID_H264:{
            H264SWDecoder* d = new H264SWDecoder();
            bool ret = d->init( para , timebase );
            if(ret) {
                return d;
            } else {
                delete d;
            }
            LOGE(LOG_TAG, "create H264SWDecoder Fail !");
            return NULL;
        }
        case AV_CODEC_ID_AAC:{
            AACSWDecoder* d = new AACSWDecoder();
            bool ret = d->init( para , timebase );
            if(ret) {
                return d;
            } else {
                delete d;
            }
            LOGE(LOG_TAG, "create AACSWDecoder Fail !");
        }
        default:
            LOGE(LOG_TAG, "unsupport format %d ", para->codec_id );
            assert(false);
            break;
    }
    return NULL;
}