LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= bugreport.cpp

LOCAL_MODULE:= bugreport

LOCAL_CFLAGS := -Wall

LOCAL_SHARED_LIBRARIES := libcutils

include $(BUILD_EXECUTABLE)
