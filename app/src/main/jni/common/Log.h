#ifndef __LOGIMP_H__
#define __LOGIMP_H__

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <jni.h>
#include <android/log.h>


#ifdef _NODEBUG  // 完全不打印,不要打印信息


#define LOGT(n, t, ...)
#define LOGD(n, t, ...)
#define LOGI(n, t, ...)
#define LOGW(n, t, ...)
#define LOGE(n, t, ...)

#define LOCAL_LOG_IMPLEMENT(p, n)
#define CLASS_LOG_DECLARE(n)
#define CLASS_LOG_IMPLEMENT(c, n)

#define TLOGT(t, ...)
#define TLOGD(t, ...)
#define TLOGI(t, ...)
#define TLOGW(t, ...)
#define TLOGE(t, ...)

#define COST_STAT_DECLARE(n)

#else       //  需要打印信息

#ifndef _LOGFILE //  选择打印到文件还是内存/标准输出
#define LOG_VERBOSE ANDROID_LOG_VERBOSE
#define LOG_DEBUG   ANDROID_LOG_DEBUG
#define LOG_INFO    ANDROID_LOG_INFO
#define LOG_WARN    ANDROID_LOG_WARN
#define LOG_ERROR   ANDROID_LOG_ERROR
#define __log_write __android_log_print
#else
#define __log_write __file_log_write
extern void __file_log_write(int level, const char *tag, const char *fmt, ...);
#endif


#ifndef _RELEASE
#define LOGT(tag, fmt, ...) __log_write(LOG_VERBOSE, tag, fmt, ##__VA_ARGS__)
#define LOGD(tag, fmt, ...) __log_write(LOG_DEBUG,   tag, fmt, ##__VA_ARGS__)
#else
#define LOGT(tag, fmt, ...)
#define LOGD(tag, fmt, ...)
#endif

#define LOGI(tag, fmt, ...) __log_write(LOG_INFO,    tag, fmt, ##__VA_ARGS__)
#define LOGW(tag, fmt, ...) __log_write(LOG_WARN,    tag, fmt, ##__VA_ARGS__)
#define LOGE(tag, fmt, ...) __log_write(LOG_ERROR,   tag, fmt, ##__VA_ARGS__)

#define LOCAL_LOG_IMPLEMENT(p, tag) static const char *p = tag
#define CLASS_LOG_DECLARE(cls) 	  static const char *s_logger
#define CLASS_LOG_IMPLEMENT(cls, tag) const char *cls::s_logger = tag

#ifndef _RELEASE
#define TLOGT(fmt, ...) __log_write(LOG_VERBOSE, s_logger, "%p  " fmt, this, ##__VA_ARGS__)
#define TLOGD(fmt, ...) __log_write(LOG_DEBUG,   s_logger, "%p  " fmt, this, ##__VA_ARGS__)
#else
#define TLOGT(fmt, ...)
#define TLOGD(fmt, ...)
#endif

#define TLOGI(fmt, ...) __log_write(LOG_INFO,    s_logger, "%p  " fmt, this, ##__VA_ARGS__)
#define TLOGW(fmt, ...) __log_write(LOG_WARN,    s_logger, "%p  " fmt, this, ##__VA_ARGS__)
#define TLOGE(fmt, ...) __log_write(LOG_ERROR,   s_logger, "%p  " fmt, this, ##__VA_ARGS__)

#endif // endif

#endif /* __LOGIMP_H__ */
