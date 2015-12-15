LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libpomp
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -Wextra -fvisibility=hidden

ifdef NDK_PROJECT_PATH
    LOCAL_CFLAGS += -DANDROID_NDK
    LOCAL_EXPORT_C_INCLUDES := $(LOCAL_EXPORT_C_INCLUDE_DIRS)
endif
LOCAL_CFLAGS += -DPOMP_API_EXPORTS

LOCAL_SRC_FILES := \
	src/pomp_addr.c \
	src/pomp_buffer.c \
	src/pomp_conn.c \
	src/pomp_ctx.c \
	src/pomp_decoder.c \
	src/pomp_encoder.c \
	src/pomp_log.c \
	src/pomp_loop.c \
	src/pomp_msg.c \
	src/pomp_prot.c \
	src/pomp_timer.c

ifdef NDK_PROJECT_PATH
include $(BUILD_STATIC_LIBRARY)
else
LOCAL_SHARED_LIBRARIES := liblog
include $(BUILD_SHARED_LIBRARY)
endif
