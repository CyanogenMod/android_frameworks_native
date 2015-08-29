LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CLANG := true
LOCAL_CFLAGS := -std=c99 -fvisibility=hidden -fstrict-aliasing
LOCAL_CFLAGS += -DLOG_TAG=\"vknulldrv\"
LOCAL_CFLAGS += -Weverything -Werror \
	-Wno-padded \
	-Wno-undef \
	-Wno-zero-length-array
LOCAL_CPPFLAGS := -std=c++1y \
	-Wno-c++98-compat-pedantic \
	-Wno-c99-extensions

LOCAL_C_INCLUDES := \
	frameworks/native/vulkan/include

LOCAL_SRC_FILES := \
	null_driver.cpp \
	null_driver_gen.cpp

LOCAL_SHARED_LIBRARIES := liblog

LOCAL_MODULE := vulkan.$(TARGET_BOARD_PLATFORM)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
