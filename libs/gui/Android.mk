LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	IGraphicBufferConsumer.cpp \
	IConsumerListener.cpp \
	BitTube.cpp \
	BufferItemConsumer.cpp \
	BufferQueue.cpp \
	ConsumerBase.cpp \
	CpuConsumer.cpp \
	DisplayEventReceiver.cpp \
	GLConsumer.cpp \
	GraphicBufferAlloc.cpp \
	GuiConfig.cpp \
	IDisplayEventConnection.cpp \
	IGraphicBufferAlloc.cpp \
	IGraphicBufferProducer.cpp \
	ISensorEventConnection.cpp \
	ISensorServer.cpp \
	ISurfaceComposer.cpp \
	ISurfaceComposerClient.cpp \
	LayerState.cpp \
	Sensor.cpp \
	SensorEventQueue.cpp \
	SensorManager.cpp \
	Surface.cpp \
	SurfaceControl.cpp \
	SurfaceComposerClient.cpp \
	SyncFeatures.cpp \

LOCAL_SHARED_LIBRARIES := \
	libbinder \
	libcutils \
	libEGL \
	libGLESv2 \
	libsync \
	libui \
	libutils \
	liblog

ifeq ($(call is-board-platform-in-list, mpq8092), true)
    LOCAL_CFLAGS            += -DVFM_AVAILABLE
endif

# Executed only on QCOM BSPs
ifeq ($(TARGET_USES_QCOM_BSP),true)
ifneq ($(TARGET_QCOM_DISPLAY_VARIANT),)
    LOCAL_C_INCLUDES += hardware/qcom/display-$(TARGET_QCOM_DISPLAY_VARIANT)/libgralloc
    LOCAL_C_INCLUDES += hardware/qcom/display-$(TARGET_QCOM_DISPLAY_VARIANT)/libqdutils
else
    LOCAL_C_INCLUDES += hardware/qcom/display/$(TARGET_BOARD_PLATFORM)/libgralloc
    LOCAL_C_INCLUDES += hardware/qcom/display/$(TARGET_BOARD_PLATFORM)/libqdutils
endif
    LOCAL_C_INCLUDES        += $(TARGET_OUT_HEADERS)/vpu/
    LOCAL_CFLAGS            += -DQCOM_BSP
    LOCAL_SHARED_LIBRARIES  += libqdMetaData
endif

ifeq ($(BOARD_EGL_SKIP_FIRST_DEQUEUE),true)
    LOCAL_CFLAGS += -DSURFACE_SKIP_FIRST_DEQUEUE
endif

ifeq ($(BOARD_USE_MHEAP_SCREENSHOT),true)
    LOCAL_CFLAGS += -DUSE_MHEAP_SCREENSHOT
endif

LOCAL_MODULE:= libgui

ifeq ($(TARGET_BOARD_PLATFORM), tegra)
	LOCAL_CFLAGS += -DDONT_USE_FENCE_SYNC
endif
ifeq ($(TARGET_BOARD_PLATFORM), tegra3)
	LOCAL_CFLAGS += -DDONT_USE_FENCE_SYNC
endif
ifeq ($(TARGET_TOROPLUS_RADIO), true)
	LOCAL_CFLAGS += -DTOROPLUS_RADIO
endif

include $(BUILD_SHARED_LIBRARY)

ifeq (,$(ONE_SHOT_MAKEFILE))
include $(call first-makefiles-under,$(LOCAL_PATH))
endif
