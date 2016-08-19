LOCAL_PATH:= $(call my-dir)

svc_c_flags =	\
	-Wall -Wextra \

ifneq ($(TARGET_USES_64_BIT_BINDER),true)
ifneq ($(TARGET_IS_64_BIT),true)
svc_c_flags += -DBINDER_IPC_32BIT=1
endif
endif

include $(CLEAR_VARS)
LOCAL_SHARED_LIBRARIES := liblog
LOCAL_SRC_FILES := bctest.c binder.c
LOCAL_CFLAGS += $(svc_c_flags)
LOCAL_MODULE := bctest
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SHARED_LIBRARIES := liblog libcutils libselinux
LOCAL_SRC_FILES := service_manager.c binder.c
LOCAL_CFLAGS += $(svc_c_flags)
LOCAL_MODULE := servicemanager
include $(BUILD_EXECUTABLE)
