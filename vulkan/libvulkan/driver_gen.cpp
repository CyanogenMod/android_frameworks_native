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
#include "loader.h"

namespace vulkan {
namespace driver {

namespace {

// clang-format off

VKAPI_ATTR void disabledDestroySurfaceKHR(VkInstance, VkSurfaceKHR, const VkAllocationCallbacks*) {
    ALOGE("VK_KHR_surface not enabled. vkDestroySurfaceKHR not executed.");
}

VKAPI_ATTR VkResult disabledGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice, uint32_t, VkSurfaceKHR, VkBool32*) {
    ALOGE("VK_KHR_surface not enabled. vkGetPhysicalDeviceSurfaceSupportKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult disabledGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice, VkSurfaceKHR, VkSurfaceCapabilitiesKHR*) {
    ALOGE("VK_KHR_surface not enabled. vkGetPhysicalDeviceSurfaceCapabilitiesKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult disabledGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice, VkSurfaceKHR, uint32_t*, VkSurfaceFormatKHR*) {
    ALOGE("VK_KHR_surface not enabled. vkGetPhysicalDeviceSurfaceFormatsKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult disabledGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice, VkSurfaceKHR, uint32_t*, VkPresentModeKHR*) {
    ALOGE("VK_KHR_surface not enabled. vkGetPhysicalDeviceSurfacePresentModesKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult disabledCreateSwapchainKHR(VkDevice, const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR*) {
    ALOGE("VK_KHR_swapchain not enabled. vkCreateSwapchainKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult checkedCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
    return (GetData(device).hook_extensions[ProcHook::KHR_swapchain]) ? CreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain) : disabledCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
}

VKAPI_ATTR void disabledDestroySwapchainKHR(VkDevice, VkSwapchainKHR, const VkAllocationCallbacks*) {
    ALOGE("VK_KHR_swapchain not enabled. vkDestroySwapchainKHR not executed.");
}

VKAPI_ATTR void checkedDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator) {
    (GetData(device).hook_extensions[ProcHook::KHR_swapchain]) ? DestroySwapchainKHR(device, swapchain, pAllocator) : disabledDestroySwapchainKHR(device, swapchain, pAllocator);
}

VKAPI_ATTR VkResult disabledGetSwapchainImagesKHR(VkDevice, VkSwapchainKHR, uint32_t*, VkImage*) {
    ALOGE("VK_KHR_swapchain not enabled. vkGetSwapchainImagesKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult checkedGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) {
    return (GetData(device).hook_extensions[ProcHook::KHR_swapchain]) ? GetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, pSwapchainImages) : disabledGetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, pSwapchainImages);
}

VKAPI_ATTR VkResult disabledAcquireNextImageKHR(VkDevice, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t*) {
    ALOGE("VK_KHR_swapchain not enabled. vkAcquireNextImageKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult checkedAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex) {
    return (GetData(device).hook_extensions[ProcHook::KHR_swapchain]) ? AcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex) : disabledAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
}

VKAPI_ATTR VkResult disabledQueuePresentKHR(VkQueue, const VkPresentInfoKHR*) {
    ALOGE("VK_KHR_swapchain not enabled. vkQueuePresentKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult checkedQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    return (GetData(queue).hook_extensions[ProcHook::KHR_swapchain]) ? QueuePresentKHR(queue, pPresentInfo) : disabledQueuePresentKHR(queue, pPresentInfo);
}

VKAPI_ATTR VkResult disabledCreateAndroidSurfaceKHR(VkInstance, const VkAndroidSurfaceCreateInfoKHR*, const VkAllocationCallbacks*, VkSurfaceKHR*) {
    ALOGE("VK_KHR_android_surface not enabled. vkCreateAndroidSurfaceKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult disabledCreateDebugReportCallbackEXT(VkInstance, const VkDebugReportCallbackCreateInfoEXT*, const VkAllocationCallbacks*, VkDebugReportCallbackEXT*) {
    ALOGE("VK_EXT_debug_report not enabled. vkCreateDebugReportCallbackEXT not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR void disabledDestroyDebugReportCallbackEXT(VkInstance, VkDebugReportCallbackEXT, const VkAllocationCallbacks*) {
    ALOGE("VK_EXT_debug_report not enabled. vkDestroyDebugReportCallbackEXT not executed.");
}

VKAPI_ATTR void disabledDebugReportMessageEXT(VkInstance, VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT, uint64_t, size_t, int32_t, const char*, const char*) {
    ALOGE("VK_EXT_debug_report not enabled. vkDebugReportMessageEXT not executed.");
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
        nullptr,
    },
    {
        "vkAcquireNextImageKHR",
        ProcHook::DEVICE,
        ProcHook::KHR_swapchain,
        reinterpret_cast<PFN_vkVoidFunction>(AcquireNextImageKHR),
        reinterpret_cast<PFN_vkVoidFunction>(disabledAcquireNextImageKHR),
        reinterpret_cast<PFN_vkVoidFunction>(checkedAcquireNextImageKHR),
    },
    {
        "vkAllocateCommandBuffers",
        ProcHook::DEVICE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(AllocateCommandBuffers),
        nullptr,
        nullptr,
    },
    {
        "vkCreateAndroidSurfaceKHR",
        ProcHook::INSTANCE,
        ProcHook::KHR_android_surface,
        reinterpret_cast<PFN_vkVoidFunction>(CreateAndroidSurfaceKHR),
        reinterpret_cast<PFN_vkVoidFunction>(disabledCreateAndroidSurfaceKHR),
        nullptr,
    },
    {
        "vkCreateDebugReportCallbackEXT",
        ProcHook::INSTANCE,
        ProcHook::EXT_debug_report,
        reinterpret_cast<PFN_vkVoidFunction>(CreateDebugReportCallbackEXT),
        reinterpret_cast<PFN_vkVoidFunction>(disabledCreateDebugReportCallbackEXT),
        nullptr,
    },
    {
        "vkCreateDevice",
        ProcHook::INSTANCE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(CreateDevice),
        nullptr,
        nullptr,
    },
    {
        "vkCreateInstance",
        ProcHook::GLOBAL,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(CreateInstance),
        nullptr,
        nullptr,
    },
    {
        "vkCreateSwapchainKHR",
        ProcHook::DEVICE,
        ProcHook::KHR_swapchain,
        reinterpret_cast<PFN_vkVoidFunction>(CreateSwapchainKHR),
        reinterpret_cast<PFN_vkVoidFunction>(disabledCreateSwapchainKHR),
        reinterpret_cast<PFN_vkVoidFunction>(checkedCreateSwapchainKHR),
    },
    {
        "vkDebugReportMessageEXT",
        ProcHook::INSTANCE,
        ProcHook::EXT_debug_report,
        reinterpret_cast<PFN_vkVoidFunction>(DebugReportMessageEXT),
        reinterpret_cast<PFN_vkVoidFunction>(disabledDebugReportMessageEXT),
        nullptr,
    },
    {
        "vkDestroyDebugReportCallbackEXT",
        ProcHook::INSTANCE,
        ProcHook::EXT_debug_report,
        reinterpret_cast<PFN_vkVoidFunction>(DestroyDebugReportCallbackEXT),
        reinterpret_cast<PFN_vkVoidFunction>(disabledDestroyDebugReportCallbackEXT),
        nullptr,
    },
    {
        "vkDestroyDevice",
        ProcHook::DEVICE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(DestroyDevice),
        nullptr,
        nullptr,
    },
    {
        "vkDestroyInstance",
        ProcHook::INSTANCE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(DestroyInstance),
        nullptr,
        nullptr,
    },
    {
        "vkDestroySurfaceKHR",
        ProcHook::INSTANCE,
        ProcHook::KHR_surface,
        reinterpret_cast<PFN_vkVoidFunction>(DestroySurfaceKHR),
        reinterpret_cast<PFN_vkVoidFunction>(disabledDestroySurfaceKHR),
        nullptr,
    },
    {
        "vkDestroySwapchainKHR",
        ProcHook::DEVICE,
        ProcHook::KHR_swapchain,
        reinterpret_cast<PFN_vkVoidFunction>(DestroySwapchainKHR),
        reinterpret_cast<PFN_vkVoidFunction>(disabledDestroySwapchainKHR),
        reinterpret_cast<PFN_vkVoidFunction>(checkedDestroySwapchainKHR),
    },
    {
        "vkEnumerateDeviceExtensionProperties",
        ProcHook::INSTANCE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(EnumerateDeviceExtensionProperties),
        nullptr,
        nullptr,
    },
    {
        "vkEnumerateInstanceExtensionProperties",
        ProcHook::GLOBAL,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(EnumerateInstanceExtensionProperties),
        nullptr,
        nullptr,
    },
    {
        "vkEnumeratePhysicalDevices",
        ProcHook::INSTANCE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(EnumeratePhysicalDevices),
        nullptr,
        nullptr,
    },
    {
        "vkGetDeviceProcAddr",
        ProcHook::DEVICE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(GetDeviceProcAddr),
        nullptr,
        nullptr,
    },
    {
        "vkGetDeviceQueue",
        ProcHook::DEVICE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(GetDeviceQueue),
        nullptr,
        nullptr,
    },
    {
        "vkGetInstanceProcAddr",
        ProcHook::INSTANCE,
        ProcHook::EXTENSION_CORE,
        reinterpret_cast<PFN_vkVoidFunction>(GetInstanceProcAddr),
        nullptr,
        nullptr,
    },
    {
        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
        ProcHook::INSTANCE,
        ProcHook::KHR_surface,
        reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceSurfaceCapabilitiesKHR),
        reinterpret_cast<PFN_vkVoidFunction>(disabledGetPhysicalDeviceSurfaceCapabilitiesKHR),
        nullptr,
    },
    {
        "vkGetPhysicalDeviceSurfaceFormatsKHR",
        ProcHook::INSTANCE,
        ProcHook::KHR_surface,
        reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceSurfaceFormatsKHR),
        reinterpret_cast<PFN_vkVoidFunction>(disabledGetPhysicalDeviceSurfaceFormatsKHR),
        nullptr,
    },
    {
        "vkGetPhysicalDeviceSurfacePresentModesKHR",
        ProcHook::INSTANCE,
        ProcHook::KHR_surface,
        reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceSurfacePresentModesKHR),
        reinterpret_cast<PFN_vkVoidFunction>(disabledGetPhysicalDeviceSurfacePresentModesKHR),
        nullptr,
    },
    {
        "vkGetPhysicalDeviceSurfaceSupportKHR",
        ProcHook::INSTANCE,
        ProcHook::KHR_surface,
        reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceSurfaceSupportKHR),
        reinterpret_cast<PFN_vkVoidFunction>(disabledGetPhysicalDeviceSurfaceSupportKHR),
        nullptr,
    },
    {
        "vkGetSwapchainGrallocUsageANDROID",
        ProcHook::DEVICE,
        ProcHook::ANDROID_native_buffer,
        nullptr,
        nullptr,
        nullptr,
    },
    {
        "vkGetSwapchainImagesKHR",
        ProcHook::DEVICE,
        ProcHook::KHR_swapchain,
        reinterpret_cast<PFN_vkVoidFunction>(GetSwapchainImagesKHR),
        reinterpret_cast<PFN_vkVoidFunction>(disabledGetSwapchainImagesKHR),
        reinterpret_cast<PFN_vkVoidFunction>(checkedGetSwapchainImagesKHR),
    },
    {
        "vkQueuePresentKHR",
        ProcHook::DEVICE,
        ProcHook::KHR_swapchain,
        reinterpret_cast<PFN_vkVoidFunction>(QueuePresentKHR),
        reinterpret_cast<PFN_vkVoidFunction>(disabledQueuePresentKHR),
        reinterpret_cast<PFN_vkVoidFunction>(checkedQueuePresentKHR),
    },
    {
        "vkQueueSignalReleaseImageANDROID",
        ProcHook::DEVICE,
        ProcHook::ANDROID_native_buffer,
        nullptr,
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

#define INIT_PROC_EXT(ext, obj, proc)           \
    do {                                        \
        if (data.hal_extensions[ProcHook::ext]) \
            INIT_PROC(obj, proc);               \
    } while (0)

bool InitDriverTable(VkInstance instance, PFN_vkGetInstanceProcAddr get_proc) {
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

bool InitDriverTable(VkDevice dev, PFN_vkGetDeviceProcAddr get_proc) {
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
