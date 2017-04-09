//
// Created by Hanloong Ho on 17-4-9.
//

#ifndef DEMO_FFMPEG_AS_DECODERFACTORY_H
#define DEMO_FFMPEG_AS_DECODERFACTORY_H

#include "Decoder.h"

extern "C"{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/error.h"
}

class DecoderFactory {
public:
    static Decoder* create(const AVCodecParameters* para, double timebase);
};


#endif //DEMO_FFMPEG_AS_DECODERFACTORY_H
