# Copyright (C) 2010 The Android Open Source Project
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
# LOCAL_SANITIZE := integer

# The static constructors and destructors in this library have not been noted to
# introduce significant overheads
LOCAL_CPPFLAGS += -Wno-exit-time-destructors
LOCAL_CPPFLAGS += -Wno-global-constructors

# We only care about compiling as C++14
LOCAL_CPPFLAGS += -Wno-c++98-compat-pedantic

# We use four-character constants for the GraphicBuffer header, and don't care
# that they're non-portable as long as they're consistent within one execution
LOCAL_CPPFLAGS += -Wno-four-char-constants

# Don't warn about struct padding
LOCAL_CPPFLAGS += -Wno-padded

LOCAL_SRC_FILES := \
	Fence.cpp \
	FrameStats.cpp \
	Gralloc1.cpp \
	Gralloc1On0Adapter.cpp \
	GraphicBuffer.cpp \
	GraphicBufferAllocator.cpp \
	GraphicBufferMapper.cpp \
	HdrCapabilities.cpp \
	PixelFormat.cpp \
	Rect.cpp \
	Region.cpp \
	UiConfig.cpp

LOCAL_SHARED_LIBRARIES := \
	libbinder \
	libcutils \
	libhardware \
	libsync \
	libutils \
	liblog

ifneq ($(BOARD_FRAMEBUFFER_FORCE_FORMAT),)
LOCAL_CFLAGS += -DFRAMEBUFFER_FORCE_FORMAT=$(BOARD_FRAMEBUFFER_FORCE_FORMAT)
endif

LOCAL_MODULE := libui

include $(BUILD_SHARED_LIBRARY)


# Include subdirectory makefiles
# ============================================================

# If we're building with ONE_SHOT_MAKEFILE (mm, mmm), then what the framework
# team really wants is to build the stuff defined by this makefile.
ifeq (,$(ONE_SHOT_MAKEFILE))
include $(call first-makefiles-under,$(LOCAL_PATH))
endif
