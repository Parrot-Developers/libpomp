LOCAL_PATH := $(call my-dir)

###############################################################################
###############################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libpomp
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Printf Oriented Message Protocol
LOCAL_CONDITIONAL_LIBRARIES := OPTIONAL:libulog

LOCAL_EXPORT_C_INCLUDES := \
	$(LOCAL_PATH)/include

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
	src/pomp_log.c \
	src/pomp_loop.c \
	src/pomp_msg.c \
	src/pomp_prot.c \
	src/pomp_timer.c

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
LOCAL_MODULE := pomp-ping-vala
LOCAL_CATEGORY_PATH := libs/pomp/examples
LOCAL_DESCRIPTION := Example code for libpomp written in vala
LOCAL_LDLIBS := -lglib-2.0 -lgio-2.0 -lgobject-2.0
LOCAL_CFLAGS := -D_GNU_SOURCE
LOCAL_VALAFLAGS := --pkg gio-2.0 --pkg posix --pkg libpomp
LOCAL_SRC_FILES := examples/ping.vala
LOCAL_LIBRARIES := glib libpomp libpomp-vala
LOCAL_DEPENDS_HOST_MODULES := host.vala
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := pomp-cli
LOCAL_CATEGORY_PATH := libs/pomp/tools
LOCAL_DESCRIPTION := Command line tool for libpomp
LOCAL_SRC_FILES := tools/pomp_cli.c
LOCAL_LIBRARIES := libpomp
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := libpomp-vala
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Vala binding for libpomp
LOCAL_INSTALL_HEADERS := libpomp.vapi:usr/share/vala/vapi/
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
	tests/pomp_test_loop.c \
	tests/pomp_test_ipc.c \
	tests/pomp_test_timer.c

LOCAL_LIBRARIES := libpomp libcunit
LOCAL_CONDITIONAL_LIBRARIES := OPTIONAL:libulog

ifeq ("$(TARGET_OS)","windows")
  LOCAL_LDLIBS += -lws2_32
endif

include $(BUILD_EXECUTABLE)

endif
