/*
 * project_utils.h
 *
 *  Created on: 2016年12月10日
 *      Author: hanlon
 */

#ifndef PROJECT_UTILS_H_
#define PROJECT_UTILS_H_

#include <stdint.h>
#include <fcntl.h>

#include <jni.h>
#include <android/log.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#define DUMP_BUFFER_IN_HEX(tips, buffer, size, tag ) \
				dumpBuffer2Hex(tips, buffer , size , ANDROID_LOG_DEBUG , tag);

void dumpBuffer2Hex(const char* tips , uint8_t* buffer , int size , android_LogPriority level , const char* tag );

inline int64_t getCurTimeMs()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (uint64_t)(now.tv_sec * 1000UL + now.tv_nsec / 1000000UL);
}

inline int64_t getCurTimeUs()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (uint64_t)(now.tv_sec * 1000UL * 1000UL + now.tv_nsec / 1000UL);
}


class WriteFile{
public:
	WriteFile(const char* filename);

	int write(uint8_t* buffer , int size);

	~WriteFile();

private:
	int mFd ;
};


class CostHelper // us
{
public:
	CostHelper(){
		this->ref = GetCurrentInUs();
	}

 	int64_t Get() const {
 		int64_t now =  GetCurrentInUs();
 		return ( now - this->ref) ;
 	}

private:
	int64_t ref;
	int64_t GetCurrentInUs() const // 因为Get是const所以Get成员函数内部调用的其他成员函数都要const
	{
		struct timespec now;
	    clock_gettime(CLOCK_MONOTONIC, &now);
	    return (int64_t)(now.tv_sec * 1000000 + now.tv_nsec / 1000);// us
	}
};
#endif /* PROJECT_UTILS_H_ */
