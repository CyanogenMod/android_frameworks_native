LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	cmd.cpp

LOCAL_SHARED_LIBRARIES := \
	libutils \
	liblog \
	libbinder
	

ifeq ($(TARGET_OS),linux)
	LOCAL_CFLAGS += -DXP_UNIX
	#LOCAL_SHARED_LIBRARIES += librt
endif

LOCAL_MODULE:= cmd

include $(BUILD_EXECUTABLE)
