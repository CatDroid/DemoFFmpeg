//
// Created by Hanloong Ho on 17-4-6.
//

#ifndef DEMO_FFMPEG_AS_LOCALPLAYER_H
#define DEMO_FFMPEG_AS_LOCALPLAYER_H


#include "player/Player.h"

class LocalPlayer : public Player {

private:
    static void* thread(void* thiz);
    void loop();
};


#endif //DEMO_FFMPEG_AS_LOCALPLAYER_H
