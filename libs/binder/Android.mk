# Copyright (C) 2009 The Android Open Source Project
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

# we have the common sources, plus some device-specific stuff
sources := \
    AppOpsManager.cpp \
    Binder.cpp \
    BpBinder.cpp \
    BufferedTextOutput.cpp \
    Debug.cpp \
    IAppOpsCallback.cpp \
    IAppOpsService.cpp \
    IBatteryStats.cpp \
    IInterface.cpp \
    IMediaResourceMonitor.cpp \
    IMemory.cpp \
    IPCThreadState.cpp \
    IPermissionController.cpp \
    IProcessInfoService.cpp \
    IResultReceiver.cpp \
    IServiceManager.cpp \
    MemoryBase.cpp \
    MemoryDealer.cpp \
    MemoryHeapBase.cpp \
    Parcel.cpp \
    PermissionCache.cpp \
    PersistableBundle.cpp \
    ProcessInfoService.cpp \
    ProcessState.cpp \
    Static.cpp \
    Status.cpp \
    TextOutput.cpp \

ifeq ($(BOARD_NEEDS_MEMORYHEAPION),true)
sources += \
    MemoryHeapIon.cpp
endif

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(BOARD_NEEDS_MEMORYHEAPION),true)
LOCAL_SHARED_LIBRARIES += libion_exynos
LOCAL_CFLAGS += -DUSE_MEMORY_HEAP_ION
LOCAL_C_INCLUDES += hardware/samsung_slsi-cm/$(TARGET_BOARD_PLATFORM)/include
endif

LOCAL_MODULE := libbinder
LOCAL_SHARED_LIBRARIES += liblog libcutils libutils

LOCAL_CLANG := true
LOCAL_SANITIZE := integer
LOCAL_SRC_FILES := $(sources)
ifneq ($(TARGET_USES_64_BIT_BINDER),true)
ifneq ($(TARGET_IS_64_BIT),true)
LOCAL_CFLAGS += -DBINDER_IPC_32BIT=1
endif
endif
LOCAL_CFLAGS += -Werror
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

ifeq ($(BOARD_NEEDS_MEMORYHEAPION),true)
LOCAL_SHARED_LIBRARIES += libion_exynos
LOCAL_CFLAGS += -DUSE_MEMORY_HEAP_ION
LOCAL_C_INCLUDES += hardware/samsung_slsi-cm/$(TARGET_BOARD_PLATFORM)/include
endif

LOCAL_MODULE := libbinder
LOCAL_STATIC_LIBRARIES += libutils
LOCAL_SRC_FILES := $(sources)
ifeq ($(BOARD_NEEDS_MEMORYHEAPPMEM),true)
LOCAL_C_INCLUDES += \
    $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_ADDITIONAL_DEPENDENCIES := \
    $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
endif
ifneq ($(TARGET_USES_64_BIT_BINDER),true)
ifneq ($(TARGET_IS_64_BIT),true)
LOCAL_CFLAGS += -DBINDER_IPC_32BIT=1
endif
endif
LOCAL_CFLAGS += -Werror
include $(BUILD_STATIC_LIBRARY)
