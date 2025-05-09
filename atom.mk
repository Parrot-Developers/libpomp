LOCAL_PATH := $(call my-dir)

###############################################################################
###############################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libpomp
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Printf Oriented Message Protocol
LOCAL_CONDITIONAL_PRIVATE_LIBRARIES := OPTIONAL:libulog

LOCAL_EXPORT_C_INCLUDES := \
	$(LOCAL_PATH)/include

# Public API headers - top level headers first
# This header list is currently used to generate a python binding
LOCAL_EXPORT_CUSTOM_VARIABLES := LIBPOMP_HEADERS=$\
	$(LOCAL_PATH)/include/libpomp.h;

# Test code needs access to internal functions
ifndef TARGET_TEST
  LOCAL_CFLAGS += -fvisibility=hidden
else ifeq ("$(TARGET_OS)","windows")
  LOCAL_LDFLAGS += -Wl,--export-all-symbols
endif
LOCAL_CFLAGS += -DPOMP_API_EXPORTS

LOCAL_SRC_FILES := \
	src/pomp_addr.c \
	src/pomp_buffer.c \
	src/pomp_conn.c \
	src/pomp_ctx.c \
	src/pomp_decoder.c \
	src/pomp_encoder.c \
	src/pomp_evt.c \
	src/pomp_log.c \
	src/pomp_loop.c \
	src/pomp_loop_sync.c \
	src/pomp_msg.c \
	src/pomp_prot.c \
	src/pomp_timer.c \
	src/pomp_watchdog.c \

ifeq ("$(TARGET_OS)","windows")
  LOCAL_LDLIBS += -lws2_32
endif

ifeq ("$(TARGET_OS_FLAVOUR)","android")
  LOCAL_LDLIBS += -llog
endif

LOCAL_DOXYFILE := Doxyfile

include $(BUILD_LIBRARY)

###############################################################################
###############################################################################

include $(CLEAR_VARS)
LOCAL_MODULE := pomp-ping
LOCAL_CATEGORY_PATH := libs/pomp/examples
LOCAL_DESCRIPTION := Example code for libpomp
LOCAL_SRC_FILES := examples/ping.c
LOCAL_LIBRARIES := libpomp
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := pomp-ping-cpp
LOCAL_CATEGORY_PATH := libs/pomp/examples
LOCAL_DESCRIPTION := Example code for libpomp written in c++
LOCAL_CXXFLAGS := -std=c++0x
LOCAL_SRC_FILES := examples/ping.cpp
LOCAL_LIBRARIES := libpomp
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := pomp-cli
LOCAL_CATEGORY_PATH := libs/pomp/tools
LOCAL_DESCRIPTION := Command line tool for libpomp
LOCAL_SRC_FILES := tools/pomp_cli.c
LOCAL_LIBRARIES := libpomp
include $(BUILD_EXECUTABLE)

###############################################################################
###############################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libpomp-py
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Python binding for libpomp

LOCAL_CUSTOM_MACROS := $\
	pybinding-macro:libpomp,libpomp,$(LOCAL_PATH)/include/libpomp.h,$\
		$(TARGET_OUT_STAGING)/$(TARGET_DEFAULT_LIB_DESTDIR)/libpomp.so

include $(BUILD_CUSTOM)

###############################################################################
###############################################################################

ifdef TARGET_TEST

include $(CLEAR_VARS)
LOCAL_MODULE := tst-pomp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/src

LOCAL_SRC_FILES := \
	tests/pomp_test.c \
	tests/pomp_test_addr.c \
	tests/pomp_test_basic.c \
	tests/pomp_test_ctx.c \
	tests/pomp_test_evt.c \
	tests/pomp_test_loop.c \
	tests/pomp_test_ipc.c \
	tests/pomp_test_timer.c \
	tests/pomp_test_nonregression.c

LOCAL_LIBRARIES := libpomp libcunit
LOCAL_CONDITIONAL_LIBRARIES := OPTIONAL:libulog

ifeq ("$(TARGET_OS)","windows")
  LOCAL_LDLIBS += -lws2_32
endif

include $(BUILD_EXECUTABLE)

endif
