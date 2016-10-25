LOCAL_PATH:= $(call my-dir)

# bugreportz
# ==========

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
   bugreportz.cpp \
   main.cpp \

LOCAL_MODULE:= bugreportz

LOCAL_CFLAGS := -Werror -Wall

LOCAL_SHARED_LIBRARIES := \
    libbase \
    libcutils \

include $(BUILD_EXECUTABLE)

# bugreportz_test
# ===============

include $(CLEAR_VARS)

LOCAL_MODULE := bugreportz_test
LOCAL_MODULE_TAGS := tests

LOCAL_CFLAGS := -Werror -Wall

LOCAL_SRC_FILES := \
    bugreportz.cpp \
    bugreportz_test.cpp \

LOCAL_STATIC_LIBRARIES := \
    libgmock \

LOCAL_SHARED_LIBRARIES := \
    libbase \
    libutils \

include $(BUILD_NATIVE_TEST)
