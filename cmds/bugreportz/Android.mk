LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= bugreportz.cpp

LOCAL_MODULE:= bugreportz

LOCAL_CFLAGS := -Wall

LOCAL_SHARED_LIBRARIES := libcutils

include $(BUILD_EXECUTABLE)
