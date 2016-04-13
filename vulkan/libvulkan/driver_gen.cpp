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

// WARNING: This file is generated. See ../README.md for instructions.

#include <string.h>
#include <algorithm>
#include <log/log.h>

#include "driver.h"

namespace vulkan {
namespace driver {

namespace {

// clang-format off

VKAPI_ATTR VkResult checkedCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
    if (GetData(device).hook_extensions[ProcHook::KHR_swapchain]) {
        return CreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
    } else {
        ALOGE("VK_KHR_swapchain not enabled. vkCreateSwapchainKHR not executed.");
        return VK_SUCCESS;
    }
}

VKAPI_ATTR void checkedDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator) {
    if (GetData(device).hook_extensions[ProcHook::KHR_swapchain]) {
        DestroySwapchainKHR(device, swapchain, pAllocator);
    } else {
        ALOGE("VK_KHR_swapchain not enabled. vkDestroySwapchainKHR not executed.");
    }
}

VKAPI_ATTR VkResult checkedGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) {
    if (GetData(device).hook_extensions[ProcHook::KHR_swapchain]) {
        return GetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, pSwapchainImages);
    } else {
        ALOGE("VK_KHR_swapchain not enabled. vkGetSwapchainImagesKHR not executed.");
        return VK_SUCCESS;
    }
}

VKAPI_ATTR VkResult checkedAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex) {
    if (GetData(device).hook_extensions[ProcHook::KHR_swapchain]) {
        return AcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
    } else {
        ALOGE("VK_KHR_swapchain not enabled. vkAcquireNextImageKHR not executed.");
        return VK_SUCCESS;
    }
}

VKAPI_ATTR VkResult checkedQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    if (GetData(queue).hook_extensions[ProcHook::KHR_swapchain]) {
        return QueuePresentKHR(queue, pPresentInfo);
    } else {
        ALOGE("VK_KHR_swapchain not enabled. vkQueuePresentKHR not executed.");
        return VK_SUCCESS;
    }
}

// clang-format on

const ProcHook g_proc_hooks[] = {
    // clang-format off
    {
        "vkAcquireImageANDROID",
        ProcHook::DEVICE,
        ProcHook::ANDROID_native_buffer,
        nullptr,
        nullptr,
    },
    {
        "vkAcquireNextImageKHR",
        ProcHook::DEVICE,
        ProcHook::KHR_swapchain,
        reinterpret_cast<PFN_vkVoidFunction>(AcquireNextImageKHR),
        reinterpret_cast<PFN_vkVoidFunction>(checkedAcquireNextImageKHR),
    },
    {
        "vkAllocateCommandBuffers",
        ProcHook::DEVICE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(AllocateCommandBuffers),
        nullptr,
    },
    {
        "vkCreateAndroidSurfaceKHR",
        ProcHook::INSTANCE,
        ProcHook::KHR_android_surface,
        reinterpret_cast<PFN_vkVoidFunction>(CreateAndroidSurfaceKHR),
        nullptr,
    },
    {
        "vkCreateDebugReportCallbackEXT",
        ProcHook::INSTANCE,
        ProcHook::EXT_debug_report,
        reinterpret_cast<PFN_vkVoidFunction>(CreateDebugReportCallbackEXT),
        nullptr,
    },
    {
        "vkCreateDevice",
        ProcHook::INSTANCE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(CreateDevice),
        nullptr,
    },
    {
        "vkCreateInstance",
        ProcHook::GLOBAL,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(CreateInstance),
        nullptr,
    },
    {
        "vkCreateSwapchainKHR",
        ProcHook::DEVICE,
        ProcHook::KHR_swapchain,
        reinterpret_cast<PFN_vkVoidFunction>(CreateSwapchainKHR),
        reinterpret_cast<PFN_vkVoidFunction>(checkedCreateSwapchainKHR),
    },
    {
        "vkDebugReportMessageEXT",
        ProcHook::INSTANCE,
        ProcHook::EXT_debug_report,
        reinterpret_cast<PFN_vkVoidFunction>(DebugReportMessageEXT),
        nullptr,
    },
    {
        "vkDestroyDebugReportCallbackEXT",
        ProcHook::INSTANCE,
        ProcHook::EXT_debug_report,
        reinterpret_cast<PFN_vkVoidFunction>(DestroyDebugReportCallbackEXT),
        nullptr,
    },
    {
        "vkDestroyDevice",
        ProcHook::DEVICE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(DestroyDevice),
        nullptr,
    },
    {
        "vkDestroyInstance",
        ProcHook::INSTANCE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(DestroyInstance),
        nullptr,
    },
    {
        "vkDestroySurfaceKHR",
        ProcHook::INSTANCE,
        ProcHook::KHR_surface,
        reinterpret_cast<PFN_vkVoidFunction>(DestroySurfaceKHR),
        nullptr,
    },
    {
        "vkDestroySwapchainKHR",
        ProcHook::DEVICE,
        ProcHook::KHR_swapchain,
        reinterpret_cast<PFN_vkVoidFunction>(DestroySwapchainKHR),
        reinterpret_cast<PFN_vkVoidFunction>(checkedDestroySwapchainKHR),
    },
    {
        "vkEnumerateDeviceExtensionProperties",
        ProcHook::INSTANCE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(EnumerateDeviceExtensionProperties),
        nullptr,
    },
    {
        "vkEnumerateInstanceExtensionProperties",
        ProcHook::GLOBAL,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(EnumerateInstanceExtensionProperties),
        nullptr,
    },
    {
        "vkEnumeratePhysicalDevices",
        ProcHook::INSTANCE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(EnumeratePhysicalDevices),
        nullptr,
    },
    {
        "vkGetDeviceProcAddr",
        ProcHook::DEVICE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(GetDeviceProcAddr),
        nullptr,
    },
    {
        "vkGetDeviceQueue",
        ProcHook::DEVICE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(GetDeviceQueue),
        nullptr,
    },
    {
        "vkGetInstanceProcAddr",
        ProcHook::INSTANCE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(GetInstanceProcAddr),
        nullptr,
    },
    {
        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
        ProcHook::INSTANCE,
        ProcHook::KHR_surface,
        reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceSurfaceCapabilitiesKHR),
        nullptr,
    },
    {
        "vkGetPhysicalDeviceSurfaceFormatsKHR",
        ProcHook::INSTANCE,
        ProcHook::KHR_surface,
        reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceSurfaceFormatsKHR),
        nullptr,
    },
    {
        "vkGetPhysicalDeviceSurfacePresentModesKHR",
        ProcHook::INSTANCE,
        ProcHook::KHR_surface,
        reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceSurfacePresentModesKHR),
        nullptr,
    },
    {
        "vkGetPhysicalDeviceSurfaceSupportKHR",
        ProcHook::INSTANCE,
        ProcHook::KHR_surface,
        reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceSurfaceSupportKHR),
        nullptr,
    },
    {
        "vkGetSwapchainGrallocUsageANDROID",
        ProcHook::DEVICE,
        ProcHook::ANDROID_native_buffer,
        nullptr,
        nullptr,
    },
    {
        "vkGetSwapchainImagesKHR",
        ProcHook::DEVICE,
        ProcHook::KHR_swapchain,
        reinterpret_cast<PFN_vkVoidFunction>(GetSwapchainImagesKHR),
        reinterpret_cast<PFN_vkVoidFunction>(checkedGetSwapchainImagesKHR),
    },
    {
        "vkQueuePresentKHR",
        ProcHook::DEVICE,
        ProcHook::KHR_swapchain,
        reinterpret_cast<PFN_vkVoidFunction>(QueuePresentKHR),
        reinterpret_cast<PFN_vkVoidFunction>(checkedQueuePresentKHR),
    },
    {
        "vkQueueSignalReleaseImageANDROID",
        ProcHook::DEVICE,
        ProcHook::ANDROID_native_buffer,
        nullptr,
        nullptr,
    },
    // clang-format on
};

}  // anonymous

const ProcHook* GetProcHook(const char* name) {
    const auto& begin = g_proc_hooks;
    const auto& end =
        g_proc_hooks + sizeof(g_proc_hooks) / sizeof(g_proc_hooks[0]);
    const auto hook = std::lower_bound(
        begin, end, name,
        [](const ProcHook& e, const char* n) { return strcmp(e.name, n) < 0; });
    return (hook < end && strcmp(hook->name, name) == 0) ? hook : nullptr;
}

ProcHook::Extension GetProcHookExtension(const char* name) {
    // clang-format off
    if (strcmp(name, "VK_ANDROID_native_buffer") == 0) return ProcHook::ANDROID_native_buffer;
    if (strcmp(name, "VK_EXT_debug_report") == 0) return ProcHook::EXT_debug_report;
    if (strcmp(name, "VK_KHR_android_surface") == 0) return ProcHook::KHR_android_surface;
    if (strcmp(name, "VK_KHR_surface") == 0) return ProcHook::KHR_surface;
    if (strcmp(name, "VK_KHR_swapchain") == 0) return ProcHook::KHR_swapchain;
    // clang-format on
    return ProcHook::EXTENSION_UNKNOWN;
}

#define UNLIKELY(expr) __builtin_expect((expr), 0)

#define INIT_PROC(obj, proc)                                           \
    do {                                                               \
        data.driver.proc =                                             \
            reinterpret_cast<PFN_vk##proc>(get_proc(obj, "vk" #proc)); \
        if (UNLIKELY(!data.driver.proc)) {                             \
            ALOGE("missing " #obj " proc: vk" #proc);                  \
            success = false;                                           \
        }                                                              \
    } while (0)

#define INIT_PROC_EXT(ext, obj, proc)  \
    do {                               \
        if (extensions[ProcHook::ext]) \
            INIT_PROC(obj, proc);      \
    } while (0)

bool InitDriverTable(VkInstance instance,
                     PFN_vkGetInstanceProcAddr get_proc,
                     const std::bitset<ProcHook::EXTENSION_COUNT>& extensions) {
    auto& data = GetData(instance);
    bool success = true;

    // clang-format off
    INIT_PROC(instance, DestroyInstance);
    INIT_PROC(instance, EnumeratePhysicalDevices);
    INIT_PROC(instance, GetInstanceProcAddr);
    INIT_PROC(instance, CreateDevice);
    INIT_PROC(instance, EnumerateDeviceLayerProperties);
    INIT_PROC(instance, EnumerateDeviceExtensionProperties);
    INIT_PROC_EXT(EXT_debug_report, instance, CreateDebugReportCallbackEXT);
    INIT_PROC_EXT(EXT_debug_report, instance, DestroyDebugReportCallbackEXT);
    INIT_PROC_EXT(EXT_debug_report, instance, DebugReportMessageEXT);
    // clang-format on

    return success;
}

bool InitDriverTable(VkDevice dev,
                     PFN_vkGetDeviceProcAddr get_proc,
                     const std::bitset<ProcHook::EXTENSION_COUNT>& extensions) {
    auto& data = GetData(dev);
    bool success = true;

    // clang-format off
    INIT_PROC(dev, GetDeviceProcAddr);
    INIT_PROC(dev, DestroyDevice);
    INIT_PROC(dev, GetDeviceQueue);
    INIT_PROC(dev, CreateImage);
    INIT_PROC(dev, DestroyImage);
    INIT_PROC(dev, AllocateCommandBuffers);
    INIT_PROC_EXT(ANDROID_native_buffer, dev, GetSwapchainGrallocUsageANDROID);
    INIT_PROC_EXT(ANDROID_native_buffer, dev, AcquireImageANDROID);
    INIT_PROC_EXT(ANDROID_native_buffer, dev, QueueSignalReleaseImageANDROID);
    // clang-format on

    return success;
}

}  // namespace driver
}  // namespace vulkan

// clang-format on
