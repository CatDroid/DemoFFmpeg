LOCAL_PATH := $(call my-dir)

FFMPEG_LIB_ROOT_PATH := /home/hanlon/Cat6/git_qq_yun/ffmpeg/android/


include $(CLEAR_VARS)
LOCAL_MODULE    := avcodec
LOCAL_SRC_FILES := $(FFMPEG_LIB_ROOT_PATH)/$(TARGET_ARCH_ABI)/lib/libavcodec.so
LOCAL_EXPORT_C_INCLUDES := $(FFMPEG_LIB_ROOT_PATH)/$(TARGET_ARCH_ABI)/include/
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := avformat
LOCAL_SRC_FILES := $(FFMPEG_LIB_ROOT_PATH)/$(TARGET_ARCH_ABI)/lib/libavformat.so
LOCAL_EXPORT_C_INCLUDES := $(FFMPEG_LIB_ROOT_PATH)/$(TARGET_ARCH_ABI)/include/
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := swresample
LOCAL_SRC_FILES := $(FFMPEG_LIB_ROOT_PATH)/$(TARGET_ARCH_ABI)/lib/libswresample.so
LOCAL_EXPORT_C_INCLUDES := $(FFMPEG_LIB_ROOT_PATH)/$(TARGET_ARCH_ABI)/include/
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := swscale
LOCAL_SRC_FILES := $(FFMPEG_LIB_ROOT_PATH)/$(TARGET_ARCH_ABI)/lib/libswscale.so
LOCAL_EXPORT_C_INCLUDES := $(FFMPEG_LIB_ROOT_PATH)/$(TARGET_ARCH_ABI)/include/
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := avutil
LOCAL_SRC_FILES := $(FFMPEG_LIB_ROOT_PATH)/$(TARGET_ARCH_ABI)/lib/libavutil.so
LOCAL_EXPORT_C_INCLUDES := $(FFMPEG_LIB_ROOT_PATH)/$(TARGET_ARCH_ABI)/include/
include $(PREBUILT_SHARED_LIBRARY)


include $(CLEAR_VARS)

# GCC编译器选项及优化提示
#LOCAL_CFLAGS += -O3 -fstrict-aliasing
#LOCAL_CFLAGS += -fprefetch-loop-arrays

# avcodec_decode_audio4 avcodec_decode_video2 is deprecated
LOCAL_CFLAGS += -Wno-deprecated-declarations

LOCAL_MODULE    := demo_ffmpeg
LOCAL_SRC_FILES += common/ffmpeg_common.cpp \
					 common/jni_common.cpp \
					 common/project_utils.cpp \
					 common/BufferManager.cpp \
					 common/Buffer.cpp \
					 common/SaveFile.cpp
LOCAL_SRC_FILES += jni_onload.cpp 
LOCAL_SRC_FILES += com_tom_ffmpegAPI_MP4player.cpp
LOCAL_SRC_FILES += demux/local/LocalFileDemuxer.cpp
LOCAL_SRC_FILES += codec/soft/decode/H264SWDecoder.cpp
LOCAL_SRC_FILES += codec/soft/decode/AACSWDecoder.cpp
LOCAL_SRC_FILES += render/AudioTrack.cpp
LOCAL_SRC_FILES += render/SurfaceView.cpp
LOCAL_SRC_FILES += render/RenderThread.cpp


LOCAL_SHARED_LIBRARIES := swscale swresample avutil avformat avcodec
LOCAL_LDLIBS := -llog -ljnigraphics -landroid -lmediandk -lOpenSLES -latomic

LOCAL_C_INCLUDES += \
		$(LOCAL_PATH)/common/ \
		$(LOCAL_PATH)/demux/local \
		$(LOCAL_PATH)/demux/ \
		$(LOCAL_PATH)/codec/soft/decode  \
		$(LOCAL_PATH)/render/
		
include $(BUILD_SHARED_LIBRARY)
