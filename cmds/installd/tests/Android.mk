# Build the unit tests for installd
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk

# Build the unit tests.
test_src_files := \
    installd_utils_test.cpp

shared_libraries := \
    libbase \
    libutils \
    libcutils \

static_libraries := \
    libinstalld \
    libdiskusage \

c_includes := \
    frameworks/native/cmds/installd

$(foreach file,$(test_src_files), \
    $(eval include $(CLEAR_VARS)) \
    $(eval LOCAL_SHARED_LIBRARIES := $(shared_libraries)) \
    $(eval LOCAL_STATIC_LIBRARIES := $(static_libraries)) \
    $(eval LOCAL_SRC_FILES := $(file)) \
    $(eval LOCAL_C_INCLUDES := $(c_includes)) \
    $(eval LOCAL_MODULE := $(notdir $(file:%.cpp=%))) \
    $(eval LOCAL_CLANG := true) \
    $(eval include $(BUILD_NATIVE_TEST)) \
)
