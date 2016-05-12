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

#include "api.h"

namespace vulkan {
namespace api {

#define UNLIKELY(expr) __builtin_expect((expr), 0)

#define INIT_PROC(obj, proc)                                           \
    do {                                                               \
        data.dispatch.proc =                                           \
            reinterpret_cast<PFN_vk##proc>(get_proc(obj, "vk" #proc)); \
        if (UNLIKELY(!data.dispatch.proc)) {                           \
            ALOGE("missing " #obj " proc: vk" #proc);                  \
            success = false;                                           \
        }                                                              \
    } while (0)

// Exported extension functions may be invoked even when their extensions
// are disabled.  Dispatch to stubs when that happens.
#define INIT_PROC_EXT(ext, obj, proc)            \
    do {                                         \
        if (extensions[driver::ProcHook::ext])   \
            INIT_PROC(obj, proc);                \
        else                                     \
            data.dispatch.proc = disabled##proc; \
    } while (0)

namespace {

// clang-format off

VKAPI_ATTR void disabledDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR, const VkAllocationCallbacks*) {
    driver::Logger(instance).Err(instance, "VK_KHR_surface not enabled. Exported vkDestroySurfaceKHR not executed.");
}

VKAPI_ATTR VkResult disabledGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice, uint32_t, VkSurfaceKHR, VkBool32*) {
    driver::Logger(physicalDevice).Err(physicalDevice, "VK_KHR_surface not enabled. Exported vkGetPhysicalDeviceSurfaceSupportKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult disabledGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR, VkSurfaceCapabilitiesKHR*) {
    driver::Logger(physicalDevice).Err(physicalDevice, "VK_KHR_surface not enabled. Exported vkGetPhysicalDeviceSurfaceCapabilitiesKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult disabledGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR, uint32_t*, VkSurfaceFormatKHR*) {
    driver::Logger(physicalDevice).Err(physicalDevice, "VK_KHR_surface not enabled. Exported vkGetPhysicalDeviceSurfaceFormatsKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult disabledGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR, uint32_t*, VkPresentModeKHR*) {
    driver::Logger(physicalDevice).Err(physicalDevice, "VK_KHR_surface not enabled. Exported vkGetPhysicalDeviceSurfacePresentModesKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult disabledCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR*) {
    driver::Logger(device).Err(device, "VK_KHR_swapchain not enabled. Exported vkCreateSwapchainKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR void disabledDestroySwapchainKHR(VkDevice device, VkSwapchainKHR, const VkAllocationCallbacks*) {
    driver::Logger(device).Err(device, "VK_KHR_swapchain not enabled. Exported vkDestroySwapchainKHR not executed.");
}

VKAPI_ATTR VkResult disabledGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR, uint32_t*, VkImage*) {
    driver::Logger(device).Err(device, "VK_KHR_swapchain not enabled. Exported vkGetSwapchainImagesKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult disabledAcquireNextImageKHR(VkDevice device, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t*) {
    driver::Logger(device).Err(device, "VK_KHR_swapchain not enabled. Exported vkAcquireNextImageKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult disabledQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR*) {
    driver::Logger(queue).Err(queue, "VK_KHR_swapchain not enabled. Exported vkQueuePresentKHR not executed.");
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult disabledCreateAndroidSurfaceKHR(VkInstance instance, const VkAndroidSurfaceCreateInfoKHR*, const VkAllocationCallbacks*, VkSurfaceKHR*) {
    driver::Logger(instance).Err(instance, "VK_KHR_android_surface not enabled. Exported vkCreateAndroidSurfaceKHR not executed.");
    return VK_SUCCESS;
}

// clang-format on

}  // anonymous

bool InitDispatchTable(
    VkInstance instance,
    PFN_vkGetInstanceProcAddr get_proc,
    const std::bitset<driver::ProcHook::EXTENSION_COUNT>& extensions) {
    auto& data = GetData(instance);
    bool success = true;

    // clang-format off
    INIT_PROC(instance, DestroyInstance);
    INIT_PROC(instance, EnumeratePhysicalDevices);
    INIT_PROC(instance, GetInstanceProcAddr);
    INIT_PROC(instance, GetPhysicalDeviceProperties);
    INIT_PROC(instance, GetPhysicalDeviceQueueFamilyProperties);
    INIT_PROC(instance, GetPhysicalDeviceMemoryProperties);
    INIT_PROC(instance, GetPhysicalDeviceFeatures);
    INIT_PROC(instance, GetPhysicalDeviceFormatProperties);
    INIT_PROC(instance, GetPhysicalDeviceImageFormatProperties);
    INIT_PROC(instance, CreateDevice);
    INIT_PROC(instance, EnumerateDeviceLayerProperties);
    INIT_PROC(instance, EnumerateDeviceExtensionProperties);
    INIT_PROC(instance, GetPhysicalDeviceSparseImageFormatProperties);
    INIT_PROC_EXT(KHR_surface, instance, DestroySurfaceKHR);
    INIT_PROC_EXT(KHR_surface, instance, GetPhysicalDeviceSurfaceSupportKHR);
    INIT_PROC_EXT(KHR_surface, instance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
    INIT_PROC_EXT(KHR_surface, instance, GetPhysicalDeviceSurfaceFormatsKHR);
    INIT_PROC_EXT(KHR_surface, instance, GetPhysicalDeviceSurfacePresentModesKHR);
    INIT_PROC_EXT(KHR_android_surface, instance, CreateAndroidSurfaceKHR);
    // clang-format on

    return success;
}

bool InitDispatchTable(
    VkDevice dev,
    PFN_vkGetDeviceProcAddr get_proc,
    const std::bitset<driver::ProcHook::EXTENSION_COUNT>& extensions) {
    auto& data = GetData(dev);
    bool success = true;

    // clang-format off
    INIT_PROC(dev, GetDeviceProcAddr);
    INIT_PROC(dev, DestroyDevice);
    INIT_PROC(dev, GetDeviceQueue);
    INIT_PROC(dev, QueueSubmit);
    INIT_PROC(dev, QueueWaitIdle);
    INIT_PROC(dev, DeviceWaitIdle);
    INIT_PROC(dev, AllocateMemory);
    INIT_PROC(dev, FreeMemory);
    INIT_PROC(dev, MapMemory);
    INIT_PROC(dev, UnmapMemory);
    INIT_PROC(dev, FlushMappedMemoryRanges);
    INIT_PROC(dev, InvalidateMappedMemoryRanges);
    INIT_PROC(dev, GetDeviceMemoryCommitment);
    INIT_PROC(dev, GetBufferMemoryRequirements);
    INIT_PROC(dev, BindBufferMemory);
    INIT_PROC(dev, GetImageMemoryRequirements);
    INIT_PROC(dev, BindImageMemory);
    INIT_PROC(dev, GetImageSparseMemoryRequirements);
    INIT_PROC(dev, QueueBindSparse);
    INIT_PROC(dev, CreateFence);
    INIT_PROC(dev, DestroyFence);
    INIT_PROC(dev, ResetFences);
    INIT_PROC(dev, GetFenceStatus);
    INIT_PROC(dev, WaitForFences);
    INIT_PROC(dev, CreateSemaphore);
    INIT_PROC(dev, DestroySemaphore);
    INIT_PROC(dev, CreateEvent);
    INIT_PROC(dev, DestroyEvent);
    INIT_PROC(dev, GetEventStatus);
    INIT_PROC(dev, SetEvent);
    INIT_PROC(dev, ResetEvent);
    INIT_PROC(dev, CreateQueryPool);
    INIT_PROC(dev, DestroyQueryPool);
    INIT_PROC(dev, GetQueryPoolResults);
    INIT_PROC(dev, CreateBuffer);
    INIT_PROC(dev, DestroyBuffer);
    INIT_PROC(dev, CreateBufferView);
    INIT_PROC(dev, DestroyBufferView);
    INIT_PROC(dev, CreateImage);
    INIT_PROC(dev, DestroyImage);
    INIT_PROC(dev, GetImageSubresourceLayout);
    INIT_PROC(dev, CreateImageView);
    INIT_PROC(dev, DestroyImageView);
    INIT_PROC(dev, CreateShaderModule);
    INIT_PROC(dev, DestroyShaderModule);
    INIT_PROC(dev, CreatePipelineCache);
    INIT_PROC(dev, DestroyPipelineCache);
    INIT_PROC(dev, GetPipelineCacheData);
    INIT_PROC(dev, MergePipelineCaches);
    INIT_PROC(dev, CreateGraphicsPipelines);
    INIT_PROC(dev, CreateComputePipelines);
    INIT_PROC(dev, DestroyPipeline);
    INIT_PROC(dev, CreatePipelineLayout);
    INIT_PROC(dev, DestroyPipelineLayout);
    INIT_PROC(dev, CreateSampler);
    INIT_PROC(dev, DestroySampler);
    INIT_PROC(dev, CreateDescriptorSetLayout);
    INIT_PROC(dev, DestroyDescriptorSetLayout);
    INIT_PROC(dev, CreateDescriptorPool);
    INIT_PROC(dev, DestroyDescriptorPool);
    INIT_PROC(dev, ResetDescriptorPool);
    INIT_PROC(dev, AllocateDescriptorSets);
    INIT_PROC(dev, FreeDescriptorSets);
    INIT_PROC(dev, UpdateDescriptorSets);
    INIT_PROC(dev, CreateFramebuffer);
    INIT_PROC(dev, DestroyFramebuffer);
    INIT_PROC(dev, CreateRenderPass);
    INIT_PROC(dev, DestroyRenderPass);
    INIT_PROC(dev, GetRenderAreaGranularity);
    INIT_PROC(dev, CreateCommandPool);
    INIT_PROC(dev, DestroyCommandPool);
    INIT_PROC(dev, ResetCommandPool);
    INIT_PROC(dev, AllocateCommandBuffers);
    INIT_PROC(dev, FreeCommandBuffers);
    INIT_PROC(dev, BeginCommandBuffer);
    INIT_PROC(dev, EndCommandBuffer);
    INIT_PROC(dev, ResetCommandBuffer);
    INIT_PROC(dev, CmdBindPipeline);
    INIT_PROC(dev, CmdSetViewport);
    INIT_PROC(dev, CmdSetScissor);
    INIT_PROC(dev, CmdSetLineWidth);
    INIT_PROC(dev, CmdSetDepthBias);
    INIT_PROC(dev, CmdSetBlendConstants);
    INIT_PROC(dev, CmdSetDepthBounds);
    INIT_PROC(dev, CmdSetStencilCompareMask);
    INIT_PROC(dev, CmdSetStencilWriteMask);
    INIT_PROC(dev, CmdSetStencilReference);
    INIT_PROC(dev, CmdBindDescriptorSets);
    INIT_PROC(dev, CmdBindIndexBuffer);
    INIT_PROC(dev, CmdBindVertexBuffers);
    INIT_PROC(dev, CmdDraw);
    INIT_PROC(dev, CmdDrawIndexed);
    INIT_PROC(dev, CmdDrawIndirect);
    INIT_PROC(dev, CmdDrawIndexedIndirect);
    INIT_PROC(dev, CmdDispatch);
    INIT_PROC(dev, CmdDispatchIndirect);
    INIT_PROC(dev, CmdCopyBuffer);
    INIT_PROC(dev, CmdCopyImage);
    INIT_PROC(dev, CmdBlitImage);
    INIT_PROC(dev, CmdCopyBufferToImage);
    INIT_PROC(dev, CmdCopyImageToBuffer);
    INIT_PROC(dev, CmdUpdateBuffer);
    INIT_PROC(dev, CmdFillBuffer);
    INIT_PROC(dev, CmdClearColorImage);
    INIT_PROC(dev, CmdClearDepthStencilImage);
    INIT_PROC(dev, CmdClearAttachments);
    INIT_PROC(dev, CmdResolveImage);
    INIT_PROC(dev, CmdSetEvent);
    INIT_PROC(dev, CmdResetEvent);
    INIT_PROC(dev, CmdWaitEvents);
    INIT_PROC(dev, CmdPipelineBarrier);
    INIT_PROC(dev, CmdBeginQuery);
    INIT_PROC(dev, CmdEndQuery);
    INIT_PROC(dev, CmdResetQueryPool);
    INIT_PROC(dev, CmdWriteTimestamp);
    INIT_PROC(dev, CmdCopyQueryPoolResults);
    INIT_PROC(dev, CmdPushConstants);
    INIT_PROC(dev, CmdBeginRenderPass);
    INIT_PROC(dev, CmdNextSubpass);
    INIT_PROC(dev, CmdEndRenderPass);
    INIT_PROC(dev, CmdExecuteCommands);
    INIT_PROC_EXT(KHR_swapchain, dev, CreateSwapchainKHR);
    INIT_PROC_EXT(KHR_swapchain, dev, DestroySwapchainKHR);
    INIT_PROC_EXT(KHR_swapchain, dev, GetSwapchainImagesKHR);
    INIT_PROC_EXT(KHR_swapchain, dev, AcquireNextImageKHR);
    INIT_PROC_EXT(KHR_swapchain, dev, QueuePresentKHR);
    // clang-format on

    return success;
}

}  // namespace api
}  // namespace vulkan

// clang-format off

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
    // call into api.cpp
    return vulkan::api::CreateInstance(pCreateInfo, pAllocator, pInstance);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator) {
    // call into api.cpp
    vulkan::api::DestroyInstance(instance, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) {
    return vulkan::api::GetData(instance).dispatch.EnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
}

__attribute__((visibility("default")))
VKAPI_ATTR PFN_vkVoidFunction vkGetDeviceProcAddr(VkDevice device, const char* pName) {
    if (device == VK_NULL_HANDLE) {
        ALOGE("invalid vkGetDeviceProcAddr(VK_NULL_HANDLE, ...) call");
        return nullptr;
    }

    static const char* const known_non_device_names[] = {
        "vkCreateAndroidSurfaceKHR",
        "vkCreateDebugReportCallbackEXT",
        "vkCreateDevice",
        "vkCreateInstance",
        "vkDebugReportMessageEXT",
        "vkDestroyDebugReportCallbackEXT",
        "vkDestroyInstance",
        "vkDestroySurfaceKHR",
        "vkEnumerateDeviceExtensionProperties",
        "vkEnumerateDeviceLayerProperties",
        "vkEnumerateInstanceExtensionProperties",
        "vkEnumerateInstanceLayerProperties",
        "vkEnumeratePhysicalDevices",
        "vkGetInstanceProcAddr",
        "vkGetPhysicalDeviceFeatures",
        "vkGetPhysicalDeviceFormatProperties",
        "vkGetPhysicalDeviceImageFormatProperties",
        "vkGetPhysicalDeviceMemoryProperties",
        "vkGetPhysicalDeviceProperties",
        "vkGetPhysicalDeviceQueueFamilyProperties",
        "vkGetPhysicalDeviceSparseImageFormatProperties",
        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
        "vkGetPhysicalDeviceSurfaceFormatsKHR",
        "vkGetPhysicalDeviceSurfacePresentModesKHR",
        "vkGetPhysicalDeviceSurfaceSupportKHR",
    };
    // clang-format on
    constexpr size_t count =
        sizeof(known_non_device_names) / sizeof(known_non_device_names[0]);
    if (!pName ||
        std::binary_search(
            known_non_device_names, known_non_device_names + count, pName,
            [](const char* a, const char* b) { return (strcmp(a, b) < 0); })) {
        vulkan::driver::Logger(device).Err(
            device, "invalid vkGetDeviceProcAddr(%p, \"%s\") call", device,
            (pName) ? pName : "(null)");
        return nullptr;
    }
    // clang-format off

    if (strcmp(pName, "vkGetDeviceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vkGetDeviceProcAddr);
    if (strcmp(pName, "vkDestroyDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vulkan::api::DestroyDevice);

    return vulkan::api::GetData(device).dispatch.GetDeviceProcAddr(device, pName);
}

__attribute__((visibility("default")))
VKAPI_ATTR PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    // global functions
    if (instance == VK_NULL_HANDLE) {
        if (strcmp(pName, "vkCreateInstance") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vulkan::api::CreateInstance);
        if (strcmp(pName, "vkEnumerateInstanceLayerProperties") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vulkan::api::EnumerateInstanceLayerProperties);
        if (strcmp(pName, "vkEnumerateInstanceExtensionProperties") == 0) return reinterpret_cast<PFN_vkVoidFunction>(vulkan::api::EnumerateInstanceExtensionProperties);

        ALOGE("invalid vkGetInstanceProcAddr(VK_NULL_HANDLE, \"%s\") call", pName);
        return nullptr;
    }

    static const struct Hook {
        const char* name;
        PFN_vkVoidFunction proc;
    } hooks[] = {
        { "vkAcquireNextImageKHR", reinterpret_cast<PFN_vkVoidFunction>(vkAcquireNextImageKHR) },
        { "vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(vkAllocateCommandBuffers) },
        { "vkAllocateDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(vkAllocateDescriptorSets) },
        { "vkAllocateMemory", reinterpret_cast<PFN_vkVoidFunction>(vkAllocateMemory) },
        { "vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkBeginCommandBuffer) },
        { "vkBindBufferMemory", reinterpret_cast<PFN_vkVoidFunction>(vkBindBufferMemory) },
        { "vkBindImageMemory", reinterpret_cast<PFN_vkVoidFunction>(vkBindImageMemory) },
        { "vkCmdBeginQuery", reinterpret_cast<PFN_vkVoidFunction>(vkCmdBeginQuery) },
        { "vkCmdBeginRenderPass", reinterpret_cast<PFN_vkVoidFunction>(vkCmdBeginRenderPass) },
        { "vkCmdBindDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(vkCmdBindDescriptorSets) },
        { "vkCmdBindIndexBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCmdBindIndexBuffer) },
        { "vkCmdBindPipeline", reinterpret_cast<PFN_vkVoidFunction>(vkCmdBindPipeline) },
        { "vkCmdBindVertexBuffers", reinterpret_cast<PFN_vkVoidFunction>(vkCmdBindVertexBuffers) },
        { "vkCmdBlitImage", reinterpret_cast<PFN_vkVoidFunction>(vkCmdBlitImage) },
        { "vkCmdClearAttachments", reinterpret_cast<PFN_vkVoidFunction>(vkCmdClearAttachments) },
        { "vkCmdClearColorImage", reinterpret_cast<PFN_vkVoidFunction>(vkCmdClearColorImage) },
        { "vkCmdClearDepthStencilImage", reinterpret_cast<PFN_vkVoidFunction>(vkCmdClearDepthStencilImage) },
        { "vkCmdCopyBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCmdCopyBuffer) },
        { "vkCmdCopyBufferToImage", reinterpret_cast<PFN_vkVoidFunction>(vkCmdCopyBufferToImage) },
        { "vkCmdCopyImage", reinterpret_cast<PFN_vkVoidFunction>(vkCmdCopyImage) },
        { "vkCmdCopyImageToBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCmdCopyImageToBuffer) },
        { "vkCmdCopyQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(vkCmdCopyQueryPoolResults) },
        { "vkCmdDispatch", reinterpret_cast<PFN_vkVoidFunction>(vkCmdDispatch) },
        { "vkCmdDispatchIndirect", reinterpret_cast<PFN_vkVoidFunction>(vkCmdDispatchIndirect) },
        { "vkCmdDraw", reinterpret_cast<PFN_vkVoidFunction>(vkCmdDraw) },
        { "vkCmdDrawIndexed", reinterpret_cast<PFN_vkVoidFunction>(vkCmdDrawIndexed) },
        { "vkCmdDrawIndexedIndirect", reinterpret_cast<PFN_vkVoidFunction>(vkCmdDrawIndexedIndirect) },
        { "vkCmdDrawIndirect", reinterpret_cast<PFN_vkVoidFunction>(vkCmdDrawIndirect) },
        { "vkCmdEndQuery", reinterpret_cast<PFN_vkVoidFunction>(vkCmdEndQuery) },
        { "vkCmdEndRenderPass", reinterpret_cast<PFN_vkVoidFunction>(vkCmdEndRenderPass) },
        { "vkCmdExecuteCommands", reinterpret_cast<PFN_vkVoidFunction>(vkCmdExecuteCommands) },
        { "vkCmdFillBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCmdFillBuffer) },
        { "vkCmdNextSubpass", reinterpret_cast<PFN_vkVoidFunction>(vkCmdNextSubpass) },
        { "vkCmdPipelineBarrier", reinterpret_cast<PFN_vkVoidFunction>(vkCmdPipelineBarrier) },
        { "vkCmdPushConstants", reinterpret_cast<PFN_vkVoidFunction>(vkCmdPushConstants) },
        { "vkCmdResetEvent", reinterpret_cast<PFN_vkVoidFunction>(vkCmdResetEvent) },
        { "vkCmdResetQueryPool", reinterpret_cast<PFN_vkVoidFunction>(vkCmdResetQueryPool) },
        { "vkCmdResolveImage", reinterpret_cast<PFN_vkVoidFunction>(vkCmdResolveImage) },
        { "vkCmdSetBlendConstants", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetBlendConstants) },
        { "vkCmdSetDepthBias", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetDepthBias) },
        { "vkCmdSetDepthBounds", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetDepthBounds) },
        { "vkCmdSetEvent", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetEvent) },
        { "vkCmdSetLineWidth", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetLineWidth) },
        { "vkCmdSetScissor", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetScissor) },
        { "vkCmdSetStencilCompareMask", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetStencilCompareMask) },
        { "vkCmdSetStencilReference", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetStencilReference) },
        { "vkCmdSetStencilWriteMask", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetStencilWriteMask) },
        { "vkCmdSetViewport", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetViewport) },
        { "vkCmdUpdateBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCmdUpdateBuffer) },
        { "vkCmdWaitEvents", reinterpret_cast<PFN_vkVoidFunction>(vkCmdWaitEvents) },
        { "vkCmdWriteTimestamp", reinterpret_cast<PFN_vkVoidFunction>(vkCmdWriteTimestamp) },
        { "vkCreateBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCreateBuffer) },
        { "vkCreateBufferView", reinterpret_cast<PFN_vkVoidFunction>(vkCreateBufferView) },
        { "vkCreateCommandPool", reinterpret_cast<PFN_vkVoidFunction>(vkCreateCommandPool) },
        { "vkCreateComputePipelines", reinterpret_cast<PFN_vkVoidFunction>(vkCreateComputePipelines) },
        { "vkCreateDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(vkCreateDescriptorPool) },
        { "vkCreateDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(vkCreateDescriptorSetLayout) },
        { "vkCreateDevice", reinterpret_cast<PFN_vkVoidFunction>(vulkan::api::CreateDevice) },
        { "vkCreateEvent", reinterpret_cast<PFN_vkVoidFunction>(vkCreateEvent) },
        { "vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(vkCreateFence) },
        { "vkCreateFramebuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCreateFramebuffer) },
        { "vkCreateGraphicsPipelines", reinterpret_cast<PFN_vkVoidFunction>(vkCreateGraphicsPipelines) },
        { "vkCreateImage", reinterpret_cast<PFN_vkVoidFunction>(vkCreateImage) },
        { "vkCreateImageView", reinterpret_cast<PFN_vkVoidFunction>(vkCreateImageView) },
        { "vkCreateInstance", nullptr },
        { "vkCreatePipelineCache", reinterpret_cast<PFN_vkVoidFunction>(vkCreatePipelineCache) },
        { "vkCreatePipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(vkCreatePipelineLayout) },
        { "vkCreateQueryPool", reinterpret_cast<PFN_vkVoidFunction>(vkCreateQueryPool) },
        { "vkCreateRenderPass", reinterpret_cast<PFN_vkVoidFunction>(vkCreateRenderPass) },
        { "vkCreateSampler", reinterpret_cast<PFN_vkVoidFunction>(vkCreateSampler) },
        { "vkCreateSemaphore", reinterpret_cast<PFN_vkVoidFunction>(vkCreateSemaphore) },
        { "vkCreateShaderModule", reinterpret_cast<PFN_vkVoidFunction>(vkCreateShaderModule) },
        { "vkCreateSwapchainKHR", reinterpret_cast<PFN_vkVoidFunction>(vkCreateSwapchainKHR) },
        { "vkDestroyBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyBuffer) },
        { "vkDestroyBufferView", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyBufferView) },
        { "vkDestroyCommandPool", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyCommandPool) },
        { "vkDestroyDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyDescriptorPool) },
        { "vkDestroyDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyDescriptorSetLayout) },
        { "vkDestroyDevice", reinterpret_cast<PFN_vkVoidFunction>(vulkan::api::DestroyDevice) },
        { "vkDestroyEvent", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyEvent) },
        { "vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyFence) },
        { "vkDestroyFramebuffer", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyFramebuffer) },
        { "vkDestroyImage", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyImage) },
        { "vkDestroyImageView", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyImageView) },
        { "vkDestroyInstance", reinterpret_cast<PFN_vkVoidFunction>(vulkan::api::DestroyInstance) },
        { "vkDestroyPipeline", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyPipeline) },
        { "vkDestroyPipelineCache", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyPipelineCache) },
        { "vkDestroyPipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyPipelineLayout) },
        { "vkDestroyQueryPool", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyQueryPool) },
        { "vkDestroyRenderPass", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyRenderPass) },
        { "vkDestroySampler", reinterpret_cast<PFN_vkVoidFunction>(vkDestroySampler) },
        { "vkDestroySemaphore", reinterpret_cast<PFN_vkVoidFunction>(vkDestroySemaphore) },
        { "vkDestroyShaderModule", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyShaderModule) },
        { "vkDestroySwapchainKHR", reinterpret_cast<PFN_vkVoidFunction>(vkDestroySwapchainKHR) },
        { "vkDeviceWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(vkDeviceWaitIdle) },
        { "vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkEndCommandBuffer) },
        { "vkEnumerateDeviceExtensionProperties", reinterpret_cast<PFN_vkVoidFunction>(vulkan::api::EnumerateDeviceExtensionProperties) },
        { "vkEnumerateDeviceLayerProperties", reinterpret_cast<PFN_vkVoidFunction>(vulkan::api::EnumerateDeviceLayerProperties) },
        { "vkEnumerateInstanceExtensionProperties", nullptr },
        { "vkEnumerateInstanceLayerProperties", nullptr },
        { "vkFlushMappedMemoryRanges", reinterpret_cast<PFN_vkVoidFunction>(vkFlushMappedMemoryRanges) },
        { "vkFreeCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(vkFreeCommandBuffers) },
        { "vkFreeDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(vkFreeDescriptorSets) },
        { "vkFreeMemory", reinterpret_cast<PFN_vkVoidFunction>(vkFreeMemory) },
        { "vkGetBufferMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(vkGetBufferMemoryRequirements) },
        { "vkGetDeviceMemoryCommitment", reinterpret_cast<PFN_vkVoidFunction>(vkGetDeviceMemoryCommitment) },
        { "vkGetDeviceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(vkGetDeviceProcAddr) },
        { "vkGetDeviceQueue", reinterpret_cast<PFN_vkVoidFunction>(vkGetDeviceQueue) },
        { "vkGetEventStatus", reinterpret_cast<PFN_vkVoidFunction>(vkGetEventStatus) },
        { "vkGetFenceStatus", reinterpret_cast<PFN_vkVoidFunction>(vkGetFenceStatus) },
        { "vkGetImageMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(vkGetImageMemoryRequirements) },
        { "vkGetImageSparseMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(vkGetImageSparseMemoryRequirements) },
        { "vkGetImageSubresourceLayout", reinterpret_cast<PFN_vkVoidFunction>(vkGetImageSubresourceLayout) },
        { "vkGetInstanceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(vkGetInstanceProcAddr) },
        { "vkGetPipelineCacheData", reinterpret_cast<PFN_vkVoidFunction>(vkGetPipelineCacheData) },
        { "vkGetQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(vkGetQueryPoolResults) },
        { "vkGetRenderAreaGranularity", reinterpret_cast<PFN_vkVoidFunction>(vkGetRenderAreaGranularity) },
        { "vkGetSwapchainImagesKHR", reinterpret_cast<PFN_vkVoidFunction>(vkGetSwapchainImagesKHR) },
        { "vkInvalidateMappedMemoryRanges", reinterpret_cast<PFN_vkVoidFunction>(vkInvalidateMappedMemoryRanges) },
        { "vkMapMemory", reinterpret_cast<PFN_vkVoidFunction>(vkMapMemory) },
        { "vkMergePipelineCaches", reinterpret_cast<PFN_vkVoidFunction>(vkMergePipelineCaches) },
        { "vkQueueBindSparse", reinterpret_cast<PFN_vkVoidFunction>(vkQueueBindSparse) },
        { "vkQueuePresentKHR", reinterpret_cast<PFN_vkVoidFunction>(vkQueuePresentKHR) },
        { "vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(vkQueueSubmit) },
        { "vkQueueWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(vkQueueWaitIdle) },
        { "vkResetCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkResetCommandBuffer) },
        { "vkResetCommandPool", reinterpret_cast<PFN_vkVoidFunction>(vkResetCommandPool) },
        { "vkResetDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(vkResetDescriptorPool) },
        { "vkResetEvent", reinterpret_cast<PFN_vkVoidFunction>(vkResetEvent) },
        { "vkResetFences", reinterpret_cast<PFN_vkVoidFunction>(vkResetFences) },
        { "vkSetEvent", reinterpret_cast<PFN_vkVoidFunction>(vkSetEvent) },
        { "vkUnmapMemory", reinterpret_cast<PFN_vkVoidFunction>(vkUnmapMemory) },
        { "vkUpdateDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(vkUpdateDescriptorSets) },
        { "vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(vkWaitForFences) },
    };
    // clang-format on
    constexpr size_t count = sizeof(hooks) / sizeof(hooks[0]);
    auto hook = std::lower_bound(
        hooks, hooks + count, pName,
        [](const Hook& h, const char* n) { return strcmp(h.name, n) < 0; });
    if (hook < hooks + count && strcmp(hook->name, pName) == 0) {
        if (!hook->proc) {
            vulkan::driver::Logger(instance).Err(
                instance, "invalid vkGetInstanceProcAddr(%p, \"%s\") call",
                instance, pName);
        }
        return hook->proc;
    }
    // clang-format off

    return vulkan::api::GetData(instance).dispatch.GetInstanceProcAddr(instance, pName);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) {
    vulkan::api::GetData(physicalDevice).dispatch.GetPhysicalDeviceProperties(physicalDevice, pProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) {
    vulkan::api::GetData(physicalDevice).dispatch.GetPhysicalDeviceQueueFamilyProperties(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
    vulkan::api::GetData(physicalDevice).dispatch.GetPhysicalDeviceMemoryProperties(physicalDevice, pMemoryProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) {
    vulkan::api::GetData(physicalDevice).dispatch.GetPhysicalDeviceFeatures(physicalDevice, pFeatures);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) {
    vulkan::api::GetData(physicalDevice).dispatch.GetPhysicalDeviceFormatProperties(physicalDevice, format, pFormatProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) {
    return vulkan::api::GetData(physicalDevice).dispatch.GetPhysicalDeviceImageFormatProperties(physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
    // call into api.cpp
    return vulkan::api::CreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) {
    // call into api.cpp
    vulkan::api::DestroyDevice(device, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkEnumerateInstanceLayerProperties(uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
    // call into api.cpp
    return vulkan::api::EnumerateInstanceLayerProperties(pPropertyCount, pProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkEnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
    // call into api.cpp
    return vulkan::api::EnumerateInstanceExtensionProperties(pLayerName, pPropertyCount, pProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
    // call into api.cpp
    return vulkan::api::EnumerateDeviceLayerProperties(physicalDevice, pPropertyCount, pProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
    // call into api.cpp
    return vulkan::api::EnumerateDeviceExtensionProperties(physicalDevice, pLayerName, pPropertyCount, pProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) {
    vulkan::api::GetData(device).dispatch.GetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
    return vulkan::api::GetData(queue).dispatch.QueueSubmit(queue, submitCount, pSubmits, fence);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkQueueWaitIdle(VkQueue queue) {
    return vulkan::api::GetData(queue).dispatch.QueueWaitIdle(queue);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkDeviceWaitIdle(VkDevice device) {
    return vulkan::api::GetData(device).dispatch.DeviceWaitIdle(device);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) {
    return vulkan::api::GetData(device).dispatch.AllocateMemory(device, pAllocateInfo, pAllocator, pMemory);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.FreeMemory(device, memory, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) {
    return vulkan::api::GetData(device).dispatch.MapMemory(device, memory, offset, size, flags, ppData);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkUnmapMemory(VkDevice device, VkDeviceMemory memory) {
    vulkan::api::GetData(device).dispatch.UnmapMemory(device, memory);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    return vulkan::api::GetData(device).dispatch.FlushMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    return vulkan::api::GetData(device).dispatch.InvalidateMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) {
    vulkan::api::GetData(device).dispatch.GetDeviceMemoryCommitment(device, memory, pCommittedMemoryInBytes);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) {
    vulkan::api::GetData(device).dispatch.GetBufferMemoryRequirements(device, buffer, pMemoryRequirements);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    return vulkan::api::GetData(device).dispatch.BindBufferMemory(device, buffer, memory, memoryOffset);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {
    vulkan::api::GetData(device).dispatch.GetImageMemoryRequirements(device, image, pMemoryRequirements);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    return vulkan::api::GetData(device).dispatch.BindImageMemory(device, image, memory, memoryOffset);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) {
    vulkan::api::GetData(device).dispatch.GetImageSparseMemoryRequirements(device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties) {
    vulkan::api::GetData(physicalDevice).dispatch.GetPhysicalDeviceSparseImageFormatProperties(physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence) {
    return vulkan::api::GetData(queue).dispatch.QueueBindSparse(queue, bindInfoCount, pBindInfo, fence);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) {
    return vulkan::api::GetData(device).dispatch.CreateFence(device, pCreateInfo, pAllocator, pFence);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroyFence(device, fence, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences) {
    return vulkan::api::GetData(device).dispatch.ResetFences(device, fenceCount, pFences);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetFenceStatus(VkDevice device, VkFence fence) {
    return vulkan::api::GetData(device).dispatch.GetFenceStatus(device, fence);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) {
    return vulkan::api::GetData(device).dispatch.WaitForFences(device, fenceCount, pFences, waitAll, timeout);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore) {
    return vulkan::api::GetData(device).dispatch.CreateSemaphore(device, pCreateInfo, pAllocator, pSemaphore);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroySemaphore(device, semaphore, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent) {
    return vulkan::api::GetData(device).dispatch.CreateEvent(device, pCreateInfo, pAllocator, pEvent);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroyEvent(device, event, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetEventStatus(VkDevice device, VkEvent event) {
    return vulkan::api::GetData(device).dispatch.GetEventStatus(device, event);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkSetEvent(VkDevice device, VkEvent event) {
    return vulkan::api::GetData(device).dispatch.SetEvent(device, event);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkResetEvent(VkDevice device, VkEvent event) {
    return vulkan::api::GetData(device).dispatch.ResetEvent(device, event);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool) {
    return vulkan::api::GetData(device).dispatch.CreateQueryPool(device, pCreateInfo, pAllocator, pQueryPool);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroyQueryPool(device, queryPool, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) {
    return vulkan::api::GetData(device).dispatch.GetQueryPoolResults(device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) {
    return vulkan::api::GetData(device).dispatch.CreateBuffer(device, pCreateInfo, pAllocator, pBuffer);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroyBuffer(device, buffer, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView) {
    return vulkan::api::GetData(device).dispatch.CreateBufferView(device, pCreateInfo, pAllocator, pView);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroyBufferView(device, bufferView, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) {
    return vulkan::api::GetData(device).dispatch.CreateImage(device, pCreateInfo, pAllocator, pImage);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroyImage(device, image, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) {
    vulkan::api::GetData(device).dispatch.GetImageSubresourceLayout(device, image, pSubresource, pLayout);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView) {
    return vulkan::api::GetData(device).dispatch.CreateImageView(device, pCreateInfo, pAllocator, pView);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroyImageView(device, imageView, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) {
    return vulkan::api::GetData(device).dispatch.CreateShaderModule(device, pCreateInfo, pAllocator, pShaderModule);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroyShaderModule(device, shaderModule, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache) {
    return vulkan::api::GetData(device).dispatch.CreatePipelineCache(device, pCreateInfo, pAllocator, pPipelineCache);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroyPipelineCache(device, pipelineCache, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache, size_t* pDataSize, void* pData) {
    return vulkan::api::GetData(device).dispatch.GetPipelineCacheData(device, pipelineCache, pDataSize, pData);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkMergePipelineCaches(VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) {
    return vulkan::api::GetData(device).dispatch.MergePipelineCaches(device, dstCache, srcCacheCount, pSrcCaches);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    return vulkan::api::GetData(device).dispatch.CreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    return vulkan::api::GetData(device).dispatch.CreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroyPipeline(device, pipeline, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) {
    return vulkan::api::GetData(device).dispatch.CreatePipelineLayout(device, pCreateInfo, pAllocator, pPipelineLayout);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroyPipelineLayout(device, pipelineLayout, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler) {
    return vulkan::api::GetData(device).dispatch.CreateSampler(device, pCreateInfo, pAllocator, pSampler);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroySampler(device, sampler, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) {
    return vulkan::api::GetData(device).dispatch.CreateDescriptorSetLayout(device, pCreateInfo, pAllocator, pSetLayout);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroyDescriptorSetLayout(device, descriptorSetLayout, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) {
    return vulkan::api::GetData(device).dispatch.CreateDescriptorPool(device, pCreateInfo, pAllocator, pDescriptorPool);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroyDescriptorPool(device, descriptorPool, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) {
    return vulkan::api::GetData(device).dispatch.ResetDescriptorPool(device, descriptorPool, flags);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) {
    return vulkan::api::GetData(device).dispatch.AllocateDescriptorSets(device, pAllocateInfo, pDescriptorSets);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) {
    return vulkan::api::GetData(device).dispatch.FreeDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
    vulkan::api::GetData(device).dispatch.UpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer) {
    return vulkan::api::GetData(device).dispatch.CreateFramebuffer(device, pCreateInfo, pAllocator, pFramebuffer);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroyFramebuffer(device, framebuffer, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    return vulkan::api::GetData(device).dispatch.CreateRenderPass(device, pCreateInfo, pAllocator, pRenderPass);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroyRenderPass(device, renderPass, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) {
    vulkan::api::GetData(device).dispatch.GetRenderAreaGranularity(device, renderPass, pGranularity);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) {
    return vulkan::api::GetData(device).dispatch.CreateCommandPool(device, pCreateInfo, pAllocator, pCommandPool);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroyCommandPool(device, commandPool, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkResetCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) {
    return vulkan::api::GetData(device).dispatch.ResetCommandPool(device, commandPool, flags);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers) {
    return vulkan::api::GetData(device).dispatch.AllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    vulkan::api::GetData(device).dispatch.FreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) {
    return vulkan::api::GetData(commandBuffer).dispatch.BeginCommandBuffer(commandBuffer, pBeginInfo);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
    return vulkan::api::GetData(commandBuffer).dispatch.EndCommandBuffer(commandBuffer);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
    return vulkan::api::GetData(commandBuffer).dispatch.ResetCommandBuffer(commandBuffer, flags);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdSetViewport(commandBuffer, firstViewport, viewportCount, pViewports);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetLineWidth(VkCommandBuffer commandBuffer, float lineWidth) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdSetLineWidth(commandBuffer, lineWidth);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdSetDepthBias(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetBlendConstants(VkCommandBuffer commandBuffer, const float blendConstants[4]) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdSetBlendConstants(commandBuffer, blendConstants);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetDepthBounds(VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdSetDepthBounds(commandBuffer, minDepthBounds, maxDepthBounds);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdSetStencilCompareMask(commandBuffer, faceMask, compareMask);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdSetStencilWriteMask(commandBuffer, faceMask, writeMask);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetStencilReference(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdSetStencilReference(commandBuffer, faceMask, reference);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdDrawIndirect(commandBuffer, buffer, offset, drawCount, stride);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdDrawIndexedIndirect(commandBuffer, buffer, offset, drawCount, stride);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t x, uint32_t y, uint32_t z) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdDispatch(commandBuffer, x, y, z);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdDispatchIndirect(commandBuffer, buffer, offset);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdCopyImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdBlitImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdCopyImageToBuffer(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const uint32_t* pData) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdUpdateBuffer(commandBuffer, dstBuffer, dstOffset, dataSize, pData);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdFillBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdFillBuffer(commandBuffer, dstBuffer, dstOffset, size, data);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdClearDepthStencilImage(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdClearAttachments(commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdResolveImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdResolveImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdSetEvent(commandBuffer, event, stageMask);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdResetEvent(commandBuffer, event, stageMask);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdWaitEvents(commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdBeginQuery(commandBuffer, queryPool, query, flags);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdEndQuery(commandBuffer, queryPool, query);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdResetQueryPool(commandBuffer, queryPool, firstQuery, queryCount);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdWriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdWriteTimestamp(commandBuffer, pipelineStage, queryPool, query);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdCopyQueryPoolResults(commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdNextSubpass(commandBuffer, contents);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdEndRenderPass(VkCommandBuffer commandBuffer) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdEndRenderPass(commandBuffer);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    vulkan::api::GetData(commandBuffer).dispatch.CmdExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(instance).dispatch.DestroySurfaceKHR(instance, surface, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported) {
    return vulkan::api::GetData(physicalDevice).dispatch.GetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface, pSupported);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pSurfaceCapabilities) {
    return vulkan::api::GetData(physicalDevice).dispatch.GetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, pSurfaceCapabilities);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pSurfaceFormatCount, VkSurfaceFormatKHR* pSurfaceFormats) {
    return vulkan::api::GetData(physicalDevice).dispatch.GetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, pSurfaceFormatCount, pSurfaceFormats);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pPresentModeCount, VkPresentModeKHR* pPresentModes) {
    return vulkan::api::GetData(physicalDevice).dispatch.GetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, pPresentModeCount, pPresentModes);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
    return vulkan::api::GetData(device).dispatch.CreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator) {
    vulkan::api::GetData(device).dispatch.DestroySwapchainKHR(device, swapchain, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) {
    return vulkan::api::GetData(device).dispatch.GetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, pSwapchainImages);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex) {
    return vulkan::api::GetData(device).dispatch.AcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    return vulkan::api::GetData(queue).dispatch.QueuePresentKHR(queue, pPresentInfo);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateAndroidSurfaceKHR(VkInstance instance, const VkAndroidSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) {
    return vulkan::api::GetData(instance).dispatch.CreateAndroidSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
}

// clang-format on
