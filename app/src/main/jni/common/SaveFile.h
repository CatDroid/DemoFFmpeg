//
// Created by hanlon on 17-1-11.
//

#ifndef DEMO_FFMPEG_AS_SAVEFILE_H
#define DEMO_FFMPEG_AS_SAVEFILE_H


#include <string>
#include "LightRefBase.h"

class SaveFile :public LightRefBase<SaveFile> {

private:
    int mFd ;
public:
    SaveFile(std::string name );
    int save(uint8_t* buf , uint32_t size );
    ~SaveFile();
};


#endif //DEMO_FFMPEG_AS_SAVEFILE_H
