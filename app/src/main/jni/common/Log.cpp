#include "Log.h"
#include <sys/time.h>
#include <pthread.h> 

#ifndef _NODEBUG //实现写日志文件接口
#ifdef _LOGFILE

#定义在中 jni_onload.cpp 中
extern FILE *_logfp;
extern void *_logmutex;

////////////////////////////////////////////////////////////////////////////////
void __file_log_write(int level, const char *n, const char *t, ...)
{
	if( _logfp == 0 ) return;

	struct timeval v;
	gettimeofday(&v, 0);
	struct tm *p = localtime(&v.tv_sec);
	char fmt[1024]; //限制t不能太大  t:格式字符串   hl.he: %.*s 可变宽域的字符串
	sprintf(fmt, "%04d/%02d/%02d %02d:%02d:%02d,%03d %d %s %.*s\n", 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec, (int)(v.tv_usec/1000), gettid(),
			level==LOG_VERBOSE? "TRACE"
			        :(level==LOG_DEBUG? "DEBUG"
			        :(level==LOG_INFO? "INFO"
			        :(level==LOG_WARN? "WARN"
			        :"ERROR")))
            strlen(t)>800?800:strlen(t), t);


	pthread_mutex_lock((pthread_mutex_t*)_logmutex);
	va_list params;
	va_start(params, t);
	vfprintf(_logfp, fmt, params);
	va_end(params);
	fflush(_logfp);
	pthread_mutex_unlock((pthread_mutex_t*)_logmutex);
}

#endif


#endif
