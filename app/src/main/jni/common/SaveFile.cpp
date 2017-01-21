//
// Created by hanlon on 17-1-11.
//

#include <fcntl.h>
#include <assert.h>
#include "SaveFile.h"

#define LOG_TAG "SaveFile"
#include "jni_common.h"


SaveFile::SaveFile(std::string name )
{
    assert( name.size() != 0 );
    mFd = open(name.c_str() , O_CREAT | O_RDWR | O_TRUNC, 0755);
    if( mFd < 0 ){
        ALOGE("open fail %d %s " , errno , strerror(errno));
    }
}

int SaveFile::save(uint8_t* buf , uint32_t size )
{
    if(mFd > 0 ){
        ssize_t ret = write(mFd, buf , size );
        if(ret < 0 ) ALOGE("write error %d %s " , errno, strerror(errno));
        return ret ;
    }else{
        ALOGE("file open fail before !");
        return -1 ;
    }
}

SaveFile::~SaveFile()
{
    if(mFd > 0 ){
        close(mFd);
    }else{
        ALOGE("file open fail before !");
    }
}

