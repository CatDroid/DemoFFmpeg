/*
 * jni_common.h
 *
 *  Created on: 2016年12月10日
 *      Author: hanlon
 */

#ifndef JNI_COMMON_H_
#define JNI_COMMON_H_

#include <jni.h>
#include <android/log.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>	// asprintf
#include <linux/types.h>

#ifdef __cplusplus

extern JavaVM *g_JVM;

#define JNI_ATTACH_JVM_WITH_NAME(jni_env , thread_name ) \
			JNIEnv *jenv; \
			JavaVMAttachArgs args; \
			args.version = JNI_VERSION_1_4;  \
			args.name = thread_name ; \
			args.group = NULL; \
			g_JVM->GetEnv((void **)&jenv, JNI_VERSION_1_4); \
			g_JVM->AttachCurrentThread(&jni_env, &args);

#define JNI_ATTACH_JVM(jni_env) \
			JNIEnv *jenv;\
			 int status = g_JVM->GetEnv((void **)&jenv, JNI_VERSION_1_4); \
			jint result = g_JVM->AttachCurrentThread(&jni_env, NULL);

#define JNI_DETACH_JVM(jni_env) 				if( jni_env ) { g_JVM->DetachCurrentThread(); jni_env = 0;}

#define JNI_GET_STATIC_METHOD(type)				g_static_method[(type)]
#define JNI_SET_STATIC_METHOD(type, method)     g_static_method[(type)] = (method)

#define JNI_FINDCLASS(name)						env->FindClass(name);

#define JNI_GET_FIELD_OBJ_ID(jclazz, name) 		env->GetFieldID(jclazz, (const char*)name,  "Ljava/lang/Object;")

#define JNI_GET_FIELD_STR_ID(jclazz, name)		env->GetFieldID(jclazz, (const char*)name,  "Ljava/lang/String;")

#define JNI_GET_FIELD_INT_ID(jclazz, name)		env->GetFieldID(jclazz, (const char*)name,  "I")

#define JNI_GET_FIELD_BOOLEAN_ID(jclazz, name)  env->GetFieldID(jclazz, (const char*)name,  "Z")

#define JNI_GET_FIELD_LONG_ID(jclazz, name)		env->GetFieldID(jclazz, (const char*)name,  "J")

#define JNI_GET_FIELD_DOUBLE_ID(jclazz, name)	env->GetFieldID(jclazz, (const char*)name,  "D")

#define JNI_GET_FIELD_BYTE_ID(jclazz, name)		env->GetFieldID(jclazz, (const char*)name,  "B")

#define JNI_GET_METHOD_ID(jclazz, name, sig)	env->GetMethodID(jclazz, name, sig)

#define JNI_GET_OBJ_FIELD(obj, methodId)		env->GetObjectField(obj, methodId)

#define JNI_GET_INT_FIELD(obj, methodId)		env->GetIntField(obj, methodId)

#define JNI_GET_LONG_FILED(obj, methodId)		env->GetLongField(obj, methodId)

#define JNI_GET_DOUBLE_FILED(obj, methodId)		env->GetDoubleField(obj, methodId)

#define JNI_GET_BYTE_FILED(obj, methodId)		env->GetByteField(obj, methodId)

#define JNI_GET_BOOLEAN_FIELD(obj, methodId)	env->GetBooleanField(obj, methodId)

#define JNI_GET_UTF_STR(str, str_obj, type)		if(NULL != str_obj) {\
													str = (type)env->GetStringUTFChars(str_obj, NULL);\
												} else {\
													str = (type)NULL;\
												}

#define JNI_GET_UTF_CHAR(str, str_obj)  		JNI_GET_UTF_STR(str, str_obj, char*);

#define JNI_RELEASE_STR_STR(str, str_obj)		if( NULL != str_obj && NULL != str) {\
													env->ReleaseStringUTFChars(str_obj, (const char*)str);\
												}

#define JNI_GET_BYTE_ARRAY(b, array)			if( NULL != array) {\
													b = (INT8S*)env->GetByteArrayElements(array, (jboolean*)NULL);\
												} else {\
														b = NULL;\
												}

#define JNI_RELEASE_BYTE_ARRAY(b, array)		if( NULL != array) {\
													env->ReleaseByteArrayElements(array, (jbyte*)b, JNI_ABORT);\
												}

#define JNI_DELETE_LOCAL_REF(obj)				if( NULL != obj) {\
													env->DeleteLocalRef(obj);\


// Copy From ALog-priv.h
#ifndef LOG_NDEBUG
#ifdef NDEBUG
#define LOG_NDEBUG 1
#else
#define LOG_NDEBUG 0
#endif
#endif

#ifndef LOG_TAG
#warning "you should define LOG_TAG in your source file. use default now!"
#define LOG_TAG "default"
#endif

#ifndef ALOG
#define ALOG(priority, tag, fmt...) \
    __android_log_print(ANDROID_##priority, tag, fmt)
#endif

#ifndef ALOGV
#if LOG_NDEBUG
#define ALOGV(...)   ((void)0)
#else
#define ALOGV(...) ((void)ALOG(LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#endif
#endif

#ifndef ALOGD
#define ALOGD(...) ((void)ALOG(LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#endif

#ifndef ALOGI
#define ALOGI(...) ((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__))
#endif

#ifndef ALOGW
#define ALOGW(...) ((void)ALOG(LOG_WARN, LOG_TAG, __VA_ARGS__))
#endif

#ifndef ALOGE
#define ALOGE(...) ((void)ALOG(LOG_ERROR, LOG_TAG, __VA_ARGS__))
#endif


// Copy and Modify From JNIHelp.h
#ifndef NELEM
#	define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))
#endif

#define jniRegisterNativeMethods(ENV,CLASS,METHODS,NUM) 						\
{ 																				\
	JNIEnv* jenv = ENV ; 														\
	const char* className = CLASS;												\
	JNINativeMethod* gMethods = METHODS;										\
	int numMethods = NUM;														\
																				\
    ALOGV("Registering %s's %d native methods...", className, numMethods);		\
    																			\
    jclass c = jenv->FindClass(className);										\
    if (c == NULL) {															\
    	ALOGE("[vortex.h] Java Class Not Found");								\
        char* msg;																\
        asprintf(&msg, "Native registration unable to find class '%s';"			\
											" aborting...", className);			\
        jenv->FatalError(msg);													\
    }																			\
    																			\
    if (jenv->RegisterNatives( c, gMethods, numMethods) < 0) {					\
    	ALOGE("[vortex.h] Register Native Function Error");						\
        char* msg;																\
        asprintf(&msg, "RegisterNatives failed for '%s';"						\
        									" aborting...", className);			\
        jenv->FatalError(msg);													\
    }																			\
}


struct field {
    const char *class_name;
    const char *field_name;
    const char *field_type;
    jfieldID   *jfield;
};


#define find_fields(ENV, FIELDS, COUNT)											\
{                                                                               \
	 JNIEnv *jenv = ENV ;                                                       \
	 field *fields = FIELDS ;                                                   \
	 int count = COUNT ;                                                        \
                                                                                \
    for (int i = 0; i < count; i++) {                                           \
        field *f = &fields[i];                                                  \
        jclass clazz = jenv->FindClass(f->class_name);                           \
        if (clazz == NULL) {                                                    \
            ALOGE("Can't find %s", f->class_name);                              \
            char* msg;                                                          \
            asprintf(&msg, "Can't find %s", f->class_name);                     \
            jenv->FatalError(msg);                                              \
        }                                                                       \
                                                                                \
        jfieldID field = jenv->GetFieldID(clazz, f->field_name, f->field_type);  \
        if (field == NULL) {                                                    \
            ALOGE("Can't find %s.%s", f->class_name, f->field_name);            \
            char* msg;                                                          \
            asprintf(&msg, "Can't find %s.%s", f->class_name, f->field_name);   \
            jenv->FatalError(msg);                                              \
        }                                                                       \
                                                                                \
        *(f->jfield) = field;                                                   \
    }                                                                           \
}


 //// JNI-Thread-related
 void DetachThread2JVM(JavaVM* vm  , JNIEnv*pEnv /* in */ );
 void AttachThread2JVM( JavaVM* vm , JNIEnv** ppEnv ,/* out */
 									const char* const threadName);
 jboolean checkCallbackThread(JavaVM* vm , JNIEnv* isTargetEnv);

 /// JNI-Exception-related
 int jniThrowIOException(JNIEnv* env, int errnum);
 int jniThrowRuntimeException(JNIEnv* env, const char* msg) ;
 int jniThrowNullPointerException(JNIEnv* env, const char* msg);
 int jniThrowException(JNIEnv* env, const char* className, const char* msg);

 // save JVM in order to attach Native Thread to JVM
 void jni_setJVM(JavaVM * jvm );

#else
// c source

#endif


#endif /* JNI_COMMON_H_ */
