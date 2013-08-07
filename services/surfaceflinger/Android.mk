LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    Client.cpp \
    DisplayDevice.cpp \
    EventThread.cpp \
    FrameTracker.cpp \
    GLExtensions.cpp \
    Layer.cpp \
    LayerDim.cpp \
    MessageQueue.cpp \
    SurfaceFlinger.cpp \
    SurfaceFlingerConsumer.cpp \
    SurfaceTextureLayer.cpp \
    Transform.cpp \
    DisplayHardware/FramebufferSurface.cpp \
    DisplayHardware/HWComposer.cpp \
    DisplayHardware/PowerHAL.cpp \
    DisplayHardware/VirtualDisplaySurface.cpp \

LOCAL_CFLAGS:= -DLOG_TAG=\"SurfaceFlinger\"
LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES

ifeq ($(TARGET_BOARD_PLATFORM),omap4)
	LOCAL_CFLAGS += -DHAS_CONTEXT_PRIORITY
endif
ifeq ($(TARGET_BOARD_PLATFORM),s5pc110)
	LOCAL_CFLAGS += -DHAS_CONTEXT_PRIORITY
	LOCAL_CFLAGS += -DNEVER_DEFAULT_TO_ASYNC_MODE
endif

ifeq ($(TARGET_DISABLE_TRIPLE_BUFFERING),true)
	LOCAL_CFLAGS += -DTARGET_DISABLE_TRIPLE_BUFFERING
endif

ifeq ($(BOARD_EGL_NEEDS_LEGACY_FB),true)
	LOCAL_CFLAGS += -DBOARD_EGL_NEEDS_LEGACY_FB
        ifeq ($(TARGET_BOARD_PLATFORM),exynos4)
	    LOCAL_CFLAGS += -DEGL_NEEDS_FNW
        endif
        ifeq ($(TARGET_QCOM_DISPLAY_VARIANT), legacy)
	    LOCAL_CFLAGS += -DEGL_NEEDS_FNW
        endif
endif

ifneq ($(NUM_FRAMEBUFFER_SURFACE_BUFFERS),)
  LOCAL_CFLAGS += -DNUM_FRAMEBUFFER_SURFACE_BUFFERS=$(NUM_FRAMEBUFFER_SURFACE_BUFFERS)
endif

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	liblog \
	libdl \
	libhardware \
	libutils \
	libEGL \
	libGLESv1_CM \
	libbinder \
	libui \
	libgui

ifeq ($(TARGET_USES_QCOM_BSP), true)
ifeq ($(TARGET_QCOM_DISPLAY_VARIANT),caf)
    LOCAL_C_INCLUDES += hardware/qcom/display-caf/libgralloc
else
    LOCAL_C_INCLUDES += hardware/qcom/display/libgralloc
endif
    LOCAL_CFLAGS += -DQCOM_BSP
endif

ifeq ($(BOARD_USES_SAMSUNG_HDMI),true)
        LOCAL_CFLAGS += -DSAMSUNG_HDMI_SUPPORT
        LOCAL_SHARED_LIBRARIES += libTVOut libhdmiclient
        LOCAL_C_INCLUDES += hardware/samsung/$(TARGET_BOARD_PLATFORM)/libhdmi/libhdmiservice
        LOCAL_C_INCLUDES += hardware/samsung/$(TARGET_BOARD_PLATFORM)/include
endif

LOCAL_MODULE:= libsurfaceflinger

include $(BUILD_SHARED_LIBRARY)

###############################################################
# uses jni which may not be available in PDK
ifneq ($(wildcard libnativehelper/include),)
include $(CLEAR_VARS)
LOCAL_CFLAGS:= -DLOG_TAG=\"SurfaceFlinger\"

LOCAL_SRC_FILES:= \
    DdmConnection.cpp

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	liblog \
	libdl

LOCAL_MODULE:= libsurfaceflinger_ddmconnection

include $(BUILD_SHARED_LIBRARY)
endif # libnativehelper
