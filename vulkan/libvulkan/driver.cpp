/*
 * Copyright 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <malloc.h>
#include <sys/prctl.h>

#include "driver.h"
#include "loader.h"

// #define ENABLE_ALLOC_CALLSTACKS 1
#if ENABLE_ALLOC_CALLSTACKS
#include <utils/CallStack.h>
#define ALOGD_CALLSTACK(...)                             \
    do {                                                 \
        ALOGD(__VA_ARGS__);                              \
        android::CallStack callstack;                    \
        callstack.update();                              \
        callstack.log(LOG_TAG, ANDROID_LOG_DEBUG, "  "); \
    } while (false)
#else
#define ALOGD_CALLSTACK(...) \
    do {                     \
    } while (false)
#endif

namespace vulkan {
namespace driver {

namespace {

hwvulkan_device_t* g_hwdevice = nullptr;

VKAPI_ATTR void* DefaultAllocate(void*,
                                 size_t size,
                                 size_t alignment,
                                 VkSystemAllocationScope) {
    void* ptr = nullptr;
    // Vulkan requires 'alignment' to be a power of two, but posix_memalign
    // additionally requires that it be at least sizeof(void*).
    int ret = posix_memalign(&ptr, std::max(alignment, sizeof(void*)), size);
    ALOGD_CALLSTACK("Allocate: size=%zu align=%zu => (%d) %p", size, alignment,
                    ret, ptr);
    return ret == 0 ? ptr : nullptr;
}

VKAPI_ATTR void* DefaultReallocate(void*,
                                   void* ptr,
                                   size_t size,
                                   size_t alignment,
                                   VkSystemAllocationScope) {
    if (size == 0) {
        free(ptr);
        return nullptr;
    }

    // TODO(jessehall): Right now we never shrink allocations; if the new
    // request is smaller than the existing chunk, we just continue using it.
    // Right now the loader never reallocs, so this doesn't matter. If that
    // changes, or if this code is copied into some other project, this should
    // probably have a heuristic to allocate-copy-free when doing so will save
    // "enough" space.
    size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
    if (size <= old_size)
        return ptr;

    void* new_ptr = nullptr;
    if (posix_memalign(&new_ptr, std::max(alignment, sizeof(void*)), size) != 0)
        return nullptr;
    if (ptr) {
        memcpy(new_ptr, ptr, std::min(old_size, size));
        free(ptr);
    }
    return new_ptr;
}

VKAPI_ATTR void DefaultFree(void*, void* ptr) {
    ALOGD_CALLSTACK("Free: %p", ptr);
    free(ptr);
}

}  // anonymous namespace

bool Debuggable() {
    return (prctl(PR_GET_DUMPABLE, 0, 0, 0, 0) >= 0);
}

bool OpenHAL() {
    if (g_hwdevice)
        return true;

    const hwvulkan_module_t* module;
    int result =
        hw_get_module("vulkan", reinterpret_cast<const hw_module_t**>(&module));
    if (result != 0) {
        ALOGE("failed to load vulkan hal: %s (%d)", strerror(-result), result);
        return false;
    }

    hwvulkan_device_t* device;
    result =
        module->common.methods->open(&module->common, HWVULKAN_DEVICE_0,
                                     reinterpret_cast<hw_device_t**>(&device));
    if (result != 0) {
        ALOGE("failed to open vulkan driver: %s (%d)", strerror(-result),
              result);
        return false;
    }

    if (!InitLoader(device)) {
        device->common.close(&device->common);
        return false;
    }

    g_hwdevice = device;

    return true;
}

const VkAllocationCallbacks& GetDefaultAllocator() {
    static const VkAllocationCallbacks kDefaultAllocCallbacks = {
        .pUserData = nullptr,
        .pfnAllocation = DefaultAllocate,
        .pfnReallocation = DefaultReallocate,
        .pfnFree = DefaultFree,
    };

    return kDefaultAllocCallbacks;
}

PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char* pName) {
    const ProcHook* hook = GetProcHook(pName);
    if (!hook)
        return g_hwdevice->GetInstanceProcAddr(instance, pName);

    if (!instance) {
        if (hook->type == ProcHook::GLOBAL)
            return hook->proc;

        ALOGE(
            "Invalid use of vkGetInstanceProcAddr to query %s without an "
            "instance",
            pName);

        // Some naughty layers expect
        //
        //   vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateDevice");
        //
        // to work.
        return (strcmp(pName, "vkCreateDevice") == 0) ? hook->proc : nullptr;
    }

    PFN_vkVoidFunction proc;

    switch (hook->type) {
        case ProcHook::INSTANCE:
            proc = (GetData(instance).hook_extensions[hook->extension])
                       ? hook->proc
                       : hook->disabled_proc;
            break;
        case ProcHook::DEVICE:
            proc = (hook->extension == ProcHook::EXTENSION_CORE)
                       ? hook->proc
                       : hook->checked_proc;
            break;
        default:
            ALOGE(
                "Invalid use of vkGetInstanceProcAddr to query %s with an "
                "instance",
                pName);
            proc = nullptr;
            break;
    }

    return proc;
}

PFN_vkVoidFunction GetDeviceProcAddr(VkDevice device, const char* pName) {
    const ProcHook* hook = GetProcHook(pName);
    if (!hook)
        return GetData(device).driver.GetDeviceProcAddr(device, pName);

    if (hook->type != ProcHook::DEVICE) {
        ALOGE("Invalid use of vkGetDeviceProcAddr to query %s", pName);
        return nullptr;
    }

    return (GetData(device).hook_extensions[hook->extension])
               ? hook->proc
               : hook->disabled_proc;
}

void GetDeviceQueue(VkDevice device,
                    uint32_t queueFamilyIndex,
                    uint32_t queueIndex,
                    VkQueue* pQueue) {
    const auto& data = GetData(device);

    data.driver.GetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
    SetData(*pQueue, data);
}

}  // namespace driver
}  // namespace vulkan
