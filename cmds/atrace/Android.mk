# Copyright 2012 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= atrace.c

LOCAL_C_INCLUDES += external/zlib
LOCAL_CFLAGS += -std=c99

LOCAL_MODULE:= atrace

LOCAL_MODULE_TAGS:= debug

LOCAL_STATIC_LIBRARIES := libz

include $(BUILD_EXECUTABLE)
