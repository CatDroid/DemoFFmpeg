//
// Created by Hanloong Ho on 17-4-9.
//

#ifndef DEMO_FFMPEG_AS_RENDER_H
#define DEMO_FFMPEG_AS_RENDER_H


#include <Buffer.h>

class Render {

public:
    virtual void ~Render() {} ;
    virtual void renderAudio(sp<Buffer> buf) = 0;
    virtual void renderVideo(sp<Buffer> buf) = 0;
    virtual void setPreview(void* native_view) = 0;
};


#endif //DEMO_FFMPEG_AS_RENDER_H
