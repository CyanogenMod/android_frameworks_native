# Build the unit tests.
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

# Build the unit tests.
test_src_files := \
    InputChannel_test.cpp \
    InputEvent_test.cpp \
    InputPublisherAndConsumer_test.cpp

shared_libraries := \
    libinput \
    libcutils \
    libutils \
    libbinder \
    libui \
    libstlport

static_libraries := \
    libgtest \
    libgtest_main

$(foreach file,$(test_src_files), \
    $(eval include $(CLEAR_VARS)) \
    $(eval LOCAL_SHARED_LIBRARIES := $(shared_libraries)) \
    $(eval LOCAL_STATIC_LIBRARIES := $(static_libraries)) \
    $(eval LOCAL_SRC_FILES := $(file)) \
    $(eval LOCAL_MODULE := $(notdir $(file:%.cpp=%))) \
    $(eval include $(BUILD_NATIVE_TEST)) \
)

# NOTE: This is a compile time test, and does not need to be
# run. All assertions are static_asserts and will fail during
# buildtime if something's wrong.
include $(CLEAR_VARS)
LOCAL_SRC_FILES := StructLayout_test.cpp
LOCAL_MODULE := StructLayout_test
LOCAL_CFLAGS := -std=c++11 -O0
LOCAL_MULTILIB := both
include $(BUILD_STATIC_LIBRARY)


# Build the manual test programs.
include $(call all-makefiles-under, $(LOCAL_PATH))
