LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    EventThread.cpp                         \
    Layer.cpp                               \
    LayerBase.cpp                           \
    LayerDim.cpp                            \
    LayerScreenshot.cpp                     \
    DisplayHardware/DisplayHardware.cpp     \
    DisplayHardware/DisplayHardwareBase.cpp \
    DisplayHardware/HWComposer.cpp          \
    DisplayHardware/PowerHAL.cpp            \
    GLExtensions.cpp                        \
    MessageQueue.cpp                        \
    SurfaceFlinger.cpp                      \
    SurfaceTextureLayer.cpp                 \
    Transform.cpp                           \

LOCAL_CFLAGS:= -DLOG_TAG=\"SurfaceFlinger\"
LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES

ifeq ($(TARGET_BOARD_PLATFORM), omap3)
	LOCAL_CFLAGS += -DNO_RGBX_8888
endif
ifeq ($(TARGET_BOARD_PLATFORM), omap4)
	LOCAL_CFLAGS += -DHAS_CONTEXT_PRIORITY
endif
ifeq ($(TARGET_BOARD_PLATFORM), s5pc110)
	LOCAL_CFLAGS += -DHAS_CONTEXT_PRIORITY
	LOCAL_CFLAGS += -DNEVER_DEFAULT_TO_ASYNC_MODE
endif

ifeq ($(TARGET_DISABLE_TRIPLE_BUFFERING), true)
	LOCAL_CFLAGS += -DTARGET_DISABLE_TRIPLE_BUFFERING
endif

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libhardware \
	libutils \
	libEGL \
	libGLESv1_CM \
	libbinder \
	libui \
	libgui

ifeq ($(BOARD_USES_QCOM_HARDWARE), true)
    LOCAL_C_INCLUDES += hardware/qcom/display/libgralloc
    LOCAL_C_INCLUDES += hardware/qcom/display/libqdutils
    LOCAL_SHARED_LIBRARIES += libqdutils
    LOCAL_CFLAGS += -DQCOM_HARDWARE
endif

# this is only needed for DDMS debugging
ifneq ($(TARGET_BUILD_PDK), true)
	LOCAL_SHARED_LIBRARIES += libdvm libandroid_runtime
	LOCAL_CLFAGS += -DDDMS_DEBUGGING
	LOCAL_SRC_FILES += DdmConnection.cpp
endif

ifeq ($(TARGET_SOC),exynos5250)
LOCAL_CFLAGS += -DSAMSUNG_EXYNOS5250
endif

ifneq ($(filter s5pc110 s5pv210,$(TARGET_SOC)),)
LOCAL_CFLAGS += -DHAS_CONTEXT_PRIORITY -DNEVER_DEFAULT_TO_ASYNC_MODE -DHWC_LAYER_DIRTY_INFO
endif

ifeq ($(BOARD_USES_SAMSUNG_HDMI),true)
LOCAL_CFLAGS += -DBOARD_USES_SAMSUNG_HDMI
LOCAL_SHARED_LIBRARIES += libTVOut libhdmiclient
LOCAL_C_INCLUDES += $(TARGET_HAL_PATH)/libhdmi/libhdmiservice
LOCAL_C_INCLUDES += $(TARGET_HAL_PATH)/include
endif

LOCAL_MODULE:= libsurfaceflinger

include $(BUILD_SHARED_LIBRARY)
