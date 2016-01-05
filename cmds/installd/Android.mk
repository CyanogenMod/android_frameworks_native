LOCAL_PATH := $(call my-dir)

common_src_files := commands.cpp globals.cpp utils.cpp
common_cflags := -Wall -Werror

#
# Static library used in testing and executable
#

include $(CLEAR_VARS)
LOCAL_MODULE := libinstalld
LOCAL_MODULE_TAGS := eng tests
LOCAL_SRC_FILES := $(common_src_files)
LOCAL_CFLAGS := $(common_cflags)
LOCAL_SHARED_LIBRARIES := \
    libbase \
    liblogwrap \
    libselinux \

LOCAL_ADDITIONAL_DEPENDENCIES += $(LOCAL_PATH)/Android.mk
LOCAL_CLANG := true
include $(BUILD_STATIC_LIBRARY)

#
# Executable
#

include $(CLEAR_VARS)
LOCAL_MODULE := installd
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(common_cflags)
LOCAL_SRC_FILES := installd.cpp $(common_src_files)
LOCAL_SHARED_LIBRARIES := \
    libbase \
    libcutils \
    liblog \
    liblogwrap \
    libselinux \

LOCAL_STATIC_LIBRARIES := libdiskusage
LOCAL_ADDITIONAL_DEPENDENCIES += $(LOCAL_PATH)/Android.mk
LOCAL_INIT_RC := installd.rc
LOCAL_CLANG := true
include $(BUILD_EXECUTABLE)

# Tests.

include $(LOCAL_PATH)/tests/Android.mk