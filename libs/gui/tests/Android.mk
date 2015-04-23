# Build the unit tests,
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk

LOCAL_CLANG := true

LOCAL_MODULE := libgui_test

LOCAL_MODULE_TAGS := tests

LOCAL_SRC_FILES := \
    BufferQueue_test.cpp \
    CpuConsumer_test.cpp \
    FillBuffer.cpp \
    GLTest.cpp \
    IGraphicBufferProducer_test.cpp \
    MultiTextureConsumer_test.cpp \
    SRGB_test.cpp \
    StreamSplitter_test.cpp \
    SurfaceTextureClient_test.cpp \
    SurfaceTextureFBO_test.cpp \
    SurfaceTextureGLThreadToGL_test.cpp \
    SurfaceTextureGLToGL_test.cpp \
    SurfaceTextureGL_test.cpp \
    SurfaceTextureMultiContextGL_test.cpp \
    Surface_test.cpp \
    TextureRenderer.cpp \

LOCAL_SHARED_LIBRARIES := \
	libEGL \
	libGLESv1_CM \
	libGLESv2 \
	libbinder \
	libcutils \
	libgui \
	libsync \
	libui \
	libutils \

# Build the binary to $(TARGET_OUT_DATA_NATIVE_TESTS)/$(LOCAL_MODULE)
# to integrate with auto-test framework.
include $(BUILD_NATIVE_TEST)

# Include subdirectory makefiles
# ============================================================

# If we're building with ONE_SHOT_MAKEFILE (mm, mmm), then what the framework
# team really wants is to build the stuff defined by this makefile.
ifeq (,$(ONE_SHOT_MAKEFILE))
include $(call first-makefiles-under,$(LOCAL_PATH))
endif
