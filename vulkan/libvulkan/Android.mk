LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CLANG := true
LOCAL_CFLAGS := -std=c99 -fvisibility=hidden -fstrict-aliasing
LOCAL_CFLAGS += -DLOG_TAG=\"vulkan\"
LOCAL_CFLAGS += -Weverything -Werror -Wno-padded -Wno-undef
LOCAL_CPPFLAGS := -std=c++1y \
	-Wno-c++98-compat-pedantic \
	-Wno-exit-time-destructors \
	-Wno-c99-extensions

LOCAL_C_INCLUDES := \
	frameworks/native/vulkan/include

LOCAL_SRC_FILES := \
	entry.cpp \
	get_proc_addr.cpp \
	loader.cpp
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk

LOCAL_SHARED_LIBRARIES := libhardware liblog

LOCAL_MODULE := libvulkan
include $(BUILD_SHARED_LIBRARY)
