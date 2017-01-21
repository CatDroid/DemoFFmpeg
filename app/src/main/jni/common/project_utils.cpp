/*
 * project_utils.cpp
 *
 *  Created on: 2016年12月10日
 *      Author: hanlon
 */

#define LOG_TAG "project_utils"
#include "jni_common.h"
#include "project_utils.h"
#include <string.h>
#include <stdio.h>

void dumpBuffer2Hex(const char* tips , uint8_t* buffer , int size , android_LogPriority level , const char* tag )
{
	char dump[256]; memset(dump,0,256);
	char* p = dump ;
	for( int z = 0 ; z< size ;z ++  ){
		int writed = sprintf( p ,"%02x," , buffer[z] );
		p += writed;
	}
	__android_log_print(level, tag, "%s %s", tips ,dump ) ;
}


WriteFile::WriteFile(const char* filename):mFd(-1)
{
	mFd = ::open("/mnt/sdcard/myh264test2.yuv",O_CREAT|O_RDWR,0755);
	if(mFd < 0){
		ALOGE("WriteFile open fail %d %s " , errno , strerror(errno));
	}
}

int WriteFile::write(uint8_t* buffer , int size)
{

	if(mFd > 0){
		int ret = 0 ;
		ret = ::write(mFd, buffer , size );
		return ret ;
	}else{
		ALOGE("WriteFile write error !");
	}
	return -1 ;
}

WriteFile::~WriteFile()
{
	if(mFd > 0 ){
		close(mFd);
	}else{
		ALOGE("WriteFile close error !");
	}

}




