LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CLANG := true
LOCAL_CFLAGS := -std=c99 -fvisibility=hidden -fstrict-aliasing
LOCAL_CFLAGS += -Weverything -Werror -Wno-padded -Wno-undef -Wno-switch-enum
LOCAL_CPPFLAGS := -std=c++1y \
	-Wno-c++98-compat-pedantic \
	-Wno-c99-extensions

LOCAL_C_INCLUDES := \
	frameworks/native/vulkan/include

LOCAL_SRC_FILES := vkinfo.cpp
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk

LOCAL_SHARED_LIBRARIES := libvulkan liblog

LOCAL_MODULE := vkinfo
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
