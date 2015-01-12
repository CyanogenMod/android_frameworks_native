LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk

LOCAL_SRC_FILES := \
    src/gltrace_api.cpp \
    src/gltrace_context.cpp \
    src/gltrace_egl.cpp \
    src/gltrace_eglapi.cpp \
    src/gltrace_fixup.cpp \
    src/gltrace_hooks.cpp \
    src/gltrace_transport.cpp \
    $(call all-proto-files-under, proto)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/../ \
    external

LOCAL_STATIC_LIBRARIES := liblzf
LOCAL_SHARED_LIBRARIES := libcutils libutils liblog

LOCAL_PROTOC_OPTIMIZE_TYPE := lite

LOCAL_CFLAGS += -DLOG_TAG=\"libGLES_trace\"

# we need to access the private Bionic header <bionic_tls.h>
LOCAL_C_INCLUDES += bionic/libc/private

LOCAL_MODULE:= libGLES_trace
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
