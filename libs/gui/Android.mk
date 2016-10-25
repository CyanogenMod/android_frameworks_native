# Copyright 2010 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CLANG := true
LOCAL_CPPFLAGS := -std=c++1y -Weverything -Werror

# The static constructors and destructors in this library have not been noted to
# introduce significant overheads
LOCAL_CPPFLAGS += -Wno-exit-time-destructors
LOCAL_CPPFLAGS += -Wno-global-constructors

# We only care about compiling as C++14
LOCAL_CPPFLAGS += -Wno-c++98-compat-pedantic

# We don't need to enumerate every case in a switch as long as a default case
# is present
LOCAL_CPPFLAGS += -Wno-switch-enum

# Allow calling variadic macros without a __VA_ARGS__ list
LOCAL_CPPFLAGS += -Wno-gnu-zero-variadic-macro-arguments

# Don't warn about struct padding
LOCAL_CPPFLAGS += -Wno-padded

LOCAL_CPPFLAGS += -DDEBUG_ONLY_CODE=$(if $(filter userdebug eng,$(TARGET_BUILD_VARIANT)),1,0)

LOCAL_SRC_FILES := \
	IGraphicBufferConsumer.cpp \
	IConsumerListener.cpp \
	BitTube.cpp \
	BufferItem.cpp \
	BufferItemConsumer.cpp \
	BufferQueue.cpp \
	BufferQueueConsumer.cpp \
	BufferQueueCore.cpp \
	BufferQueueProducer.cpp \
	BufferSlot.cpp \
	ConsumerBase.cpp \
	CpuConsumer.cpp \
	DisplayEventReceiver.cpp \
	GLConsumer.cpp \
	GraphicBufferAlloc.cpp \
	GuiConfig.cpp \
	IDisplayEventConnection.cpp \
	IGraphicBufferAlloc.cpp \
	IGraphicBufferProducer.cpp \
	IProducerListener.cpp \
	ISensorEventConnection.cpp \
	ISensorServer.cpp \
	ISurfaceComposer.cpp \
	ISurfaceComposerClient.cpp \
	LayerState.cpp \
	OccupancyTracker.cpp \
	Sensor.cpp \
	SensorEventQueue.cpp \
	SensorManager.cpp \
	StreamSplitter.cpp \
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


LOCAL_MODULE := libgui

ifeq ($(TARGET_BOARD_PLATFORM), tegra)
	LOCAL_CFLAGS += -DDONT_USE_FENCE_SYNC
endif
ifeq ($(TARGET_BOARD_PLATFORM), tegra3)
	LOCAL_CFLAGS += -DDONT_USE_FENCE_SYNC
endif

ifeq ($(TARGET_NO_SENSOR_PERMISSION_CHECK),true)
LOCAL_CPPFLAGS += -DNO_SENSOR_PERMISSION_CHECK
endif

ifeq ($(TARGET_FORCE_SCREENSHOT_CPU_PATH),true)
LOCAL_CPPFLAGS += -DFORCE_SCREENSHOT_CPU_PATH
endif

include $(BUILD_SHARED_LIBRARY)

ifeq (,$(ONE_SHOT_MAKEFILE))
include $(call first-makefiles-under,$(LOCAL_PATH))
endif
